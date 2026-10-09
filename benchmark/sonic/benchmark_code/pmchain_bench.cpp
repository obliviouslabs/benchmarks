#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "common.h"
#include "sonic/omap/o2th/client.hpp"
#include "sonic/omap/pmchain/client.hpp"
#include "sonic/omap/pmchain/util/zingoram_setup.hpp"
#include "sonic/oram/adapter/direct_block.hpp"
#include "sonic/oram/zingoram/client.hpp"
#include "sonic/threads/platform/pthread_thread_pool.hpp"
#include "sonic/threads/thread_team.hpp"

static uint8_t payload_byte(uint64_t key, size_t byte) {
    // Encode the whole key, not just its low byte, and distinguish key 0 from
    // an unwritten all-zero block. No N-sized non-oblivious shadow map is needed.
    return static_cast<uint8_t>((key >> (8 * (byte % sizeof(key)))) ^ (0xa5U + byte));
}

template <size_t ValueBytes>
int benchmark_pmchain(uint64_t n, uint64_t batch_size, uint32_t threads) {
    constexpr uint64_t minimum_accesses = UINT64_C(1) << 21;
    constexpr uint64_t minimum_batches = 32;
    using Traits = sn::oram::zingoram::traits<ValueBytes, sn::oram::zingoram::epoch_mode::disjoint_epoch>;
    using Backing = sn::oram::zingoram::client<Traits>;
    using Adapter = sn::oram::adapter::direct_block<Backing>;
    using Posmap = sn::omap::o2th::o2th_rwkv<uint64_t, 8>;
    using Chain = sn::omap::pmchain::client<Posmap, Adapter>;

    const uint64_t batches = std::max(minimum_batches, (minimum_accesses + batch_size - 1) / batch_size);
    const uint64_t accesses = batches * batch_size;
    const uint64_t memory_before = getMemValue();
    const uint64_t init_start = current_time_ns();

    sn::omap::pmchain::util::zingoram_config_input config{};
    config.block_count = n;
    config.batch_size = batch_size;
    // Fixed settings from the artifact's PMCHAIN experiment.
    config.bucket_real_size = 14;
    config.bucket_dummy_size = 10;
    config.routing_depth = 5;
    config.evict_batch = 4;
    config.access_concurrency = threads;
    // Upstream rounds the epoch window to fit the whole batch and eviction rate.
    const auto setup = sn::omap::pmchain::util::compute_zingoram_setup<Traits>(config);
    // Share workers across position-map processing, ORAM queries, and eviction.
    sn::threads::pthread_thread_pool pool(threads - 1);
    Backing backing(setup.opts, sn::threads::thread_team(pool.pool(), threads));
    Adapter adapter(backing, {.block_count = n, .disjoint_epoch_window = setup.disjoint_window});
    Posmap posmap({.block_count = batch_size, .bucket_size = 64},
                  sn::threads::thread_team(pool.pool(), threads));
    sn::omap::pmchain::config chain_config{};
    chain_config.block_count = n;
    chain_config.oram_block_bytes = ValueBytes;
    chain_config.batch_size = batch_size;
    chain_config.oram_parallelism = threads;
    chain_config.drop_epoch = false;
    Chain chain(chain_config, posmap, adapter, sn::threads::thread_team(pool.pool(), threads));
    chain.initialize();

    std::vector<typename Chain::operation> operations(batch_size);
    auto execute_queries = [&] {
        chain.populate_requests({operations.data(), operations.size()});
        chain.execute_o2th_chains();
        chain.sort_o2th_chains();
        chain.execute_oram_queries();
    };
    auto check_responses = [&] {
        if (backing.pending_epoch_accesses() != 0) {
            std::fprintf(stderr, "SONIC PMCHAIN left an unflushed epoch\n");
            return false;
        }
        const auto retrieved = chain.retrieved_requests();
        for (size_t slot = 0; slot < batch_size; ++slot) {
            const auto &request = operations[slot];
            const auto &response = retrieved[slot];
            if (response.is_dummy || response.value.key != request.key ||
                response.value.extra_data != slot || response.value.is_write != request.is_write) {
                std::fprintf(stderr, "SONIC PMCHAIN returned an incorrect response at slot %zu\n", slot);
                return false;
            }
            if (request.is_write) continue;
            const auto payload = chain.request_buffer(slot);
            for (size_t b = 0; b < ValueBytes; ++b) {
                if (payload[b] != payload_byte(request.key, b)) {
                    std::fprintf(stderr, "SONIC PMCHAIN returned an incorrect payload at slot %zu\n", slot);
                    return false;
                }
            }
        }
        return true;
    };

    // Initialize B distinct hot keys spread over the N-entry position map.
    // PMCHAIN requires distinct keys within each disjoint-epoch batch. This is
    // not a fully populated map: all subsequent reads target these B keys.
    for (size_t slot = 0; slot < batch_size; ++slot) {
        auto &op = operations[slot];
        op.key = slot * (n / batch_size);
        op.is_write = true;
        op.extra_data = static_cast<uint32_t>(slot);
        auto payload = chain.request_buffer(slot);
        for (size_t b = 0; b < ValueBytes; ++b)
            payload[b] = payload_byte(op.key, b);
    }
    execute_queries();
    chain.flush_pending();
    if (!check_responses()) return 1;
    const uint64_t init_end = current_time_ns();

    uint64_t query_ns = 0;
    uint64_t flush_ns = 0;
    const uint64_t read_start = current_time_ns();
    for (uint64_t batch = 0; batch < batches; ++batch) {
        for (size_t slot = 0; slot < batch_size; ++slot) {
            auto &op = operations[slot];
            // Rotate response order, retaining distinct keys within the batch.
            op.key = ((slot + batch) % batch_size) * (n / batch_size);
            op.is_write = false;
            auto payload = chain.request_buffer(slot);
            // Poison output so a silently omitted read cannot pass validation.
            for (size_t b = 0; b < ValueBytes; ++b)
                payload[b] = static_cast<uint8_t>(~payload_byte(op.key, b));
        }
        const uint64_t start = current_time_ns();
        execute_queries();
        const uint64_t queried = current_time_ns();
        chain.flush_pending(); // Real writeback/eviction; never drop_epoch().
        const uint64_t flushed = current_time_ns();
        query_ns += queried - start;
        flush_ns += flushed - queried;
        if (!check_responses()) return 1;
    }
    const uint64_t read_end = current_time_ns();
    const uint64_t memory_after = getMemValue();
    const uint64_t memory_kb = memory_after > memory_before ? memory_after - memory_before : 0;
    std::fprintf(stderr,
        "REPORT_123UnorderedMap|sonic_pmchain|%s| N := %" PRIu64 " | Key_bytes := 8 | Value_bytes := %zu"
        " | Batch_size := %" PRIu64 " | Batches := %" PRIu64 " | Accesses := %" PRIu64
        " | Populated_keys := %" PRIu64 " | Threads := %u | Oram_threads := %u | Eviction_threads := %u"
        " | Z := %u | S := %u | R := %u | E := %u | Posmap_bucket_size := %zu"
        " | Disjoint_epoch_window := %" PRIu64 " | Initialization_time_us := %.2f"
        " | Get_latency_us := %.6f | Get_throughput_qps := %.2f"
        " | Get_burst_latency_us := %.6f | Get_writeback_latency_us := %.6f | Memory_kb := %" PRIu64
        "REPORT_123\n",
        version_string(), n, ValueBytes, batch_size, batches, accesses, batch_size,
        threads, threads, threads, config.bucket_real_size, config.bucket_dummy_size,
        config.routing_depth, config.evict_batch, posmap.bucket_size(), setup.disjoint_window,
        (init_end - init_start) / 1000.0,
        (query_ns + flush_ns) / (1000.0 * batches), accesses * 1e9 / (read_end - read_start),
        query_ns / (1000.0 * batches), flush_ns / (1000.0 * batches), memory_kb);
    return 0;
}

int main() {
    constexpr uint32_t thread_counts[] = {32};
    constexpr uint64_t batch_sizes[] = {1024, 4096, 8192, 65536, (1<<20)};

    // R=5 and E=4 require N >= 2048 for a non-empty eviction subpath.
    for (unsigned exponent = 11; exponent <= 28; ++exponent) {
        const uint64_t n = UINT64_C(1) << exponent;
        for (const uint32_t threads : thread_counts) {
            for (const uint64_t batch_size : batch_sizes) {
                if (batch_size > n) continue;
                if (batch_size == 1024 && exponent >= 24) continue;
                if (batch_size == 4096 && exponent >= 26) continue;
                RUN_TEST_FORKED(benchmark_pmchain<8>(n, batch_size, threads),
                    "N=%" PRIu64 ",key_bytes=8,value_bytes=8,batch_size=%" PRIu64 ",threads=%u",
                    n, batch_size, threads);
                RUN_TEST_FORKED(benchmark_pmchain<56>(n, batch_size, threads),
                    "N=%" PRIu64 ",key_bytes=8,value_bytes=56,batch_size=%" PRIu64 ",threads=%u",
                    n, batch_size, threads);
            }
        }
    }
    return 0;
}
