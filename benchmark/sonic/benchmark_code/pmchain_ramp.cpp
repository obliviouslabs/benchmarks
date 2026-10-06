#include <algorithm>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "ramp_latency.h"
#include "sonic/omap/o2th/client.hpp"
#include "sonic/omap/pmchain/client.hpp"
#include "sonic/omap/pmchain/util/zingoram_setup.hpp"
#include "sonic/oram/adapter/direct_block.hpp"
#include "sonic/oram/zingoram/client.hpp"
#include "sonic/threads/platform/pthread_thread_pool.hpp"
#include "sonic/threads/thread_team.hpp"

using Traits = sn::oram::zingoram::traits<8, sn::oram::zingoram::epoch_mode::disjoint_epoch>;
using Backing = sn::oram::zingoram::client<Traits>;
using Adapter = sn::oram::adapter::direct_block<Backing>;
using Posmap = sn::omap::o2th::o2th_rwkv<uint64_t, 8>;
using Chain = sn::omap::pmchain::client<Posmap, Adapter>;

struct RampContext {
    Chain &chain;
    Backing &backing;
    uint64_t map_size;
    std::vector<Chain::operation> operations;

    int access_batch(uint64_t first_query_id, uint64_t count, bool write) {
        if (count == 0 || count > operations.size()) return EINVAL;

        // PMCHAIN has a fixed physical batch size. Real requests occupy the
        // prefix; pad the remainder with dummies, without waiting for arrivals.
        // Consecutive keys modulo N are distinct within each batch (B <= N).
        for (size_t slot = 0; slot < operations.size(); ++slot) {
            auto &op = operations[slot];
            op.key = (first_query_id % map_size + slot) % map_size;
            op.is_write = write;
            op.is_dummy = slot >= count;
            op.extra_data = static_cast<uint32_t>(slot);
            // Distinguish key 0 from unwritten zeros; poison read buffers so
            // omitted reads cannot pass validation with a previous response.
            const uint64_t expected = op.key ^ UINT64_C(0xa5a5a5a5a5a5a5a5);
            const uint64_t value = write ? expected : ~expected;
            std::memcpy(chain.request_buffer(slot).data(), &value, sizeof(value));
        }

        chain.populate_requests({operations.data(), operations.size()});
        chain.execute_o2th_chains();
        chain.sort_o2th_chains();
        chain.execute_oram_queries();
        chain.flush_pending(); // Include real eviction before reporting completion.
        if (backing.pending_epoch_accesses() != 0) {
            std::fprintf(stderr, "SONIC PMCHAIN left an unflushed epoch\n");
            return EIO;
        }

        const auto responses = chain.retrieved_requests();
        for (size_t slot = 0; slot < operations.size(); ++slot) {
            const auto &op = operations[slot];
            const auto &response = responses[slot];
            if (response.is_dummy != op.is_dummy ||
                (!op.is_dummy && (response.value.key != op.key ||
                                 response.value.extra_data != slot ||
                                 response.value.is_write != write))) {
                std::fprintf(stderr, "SONIC PMCHAIN returned an incorrect response at slot %zu\n", slot);
                return EIO;
            }
            if (op.is_dummy || write) continue;
            uint64_t value;
            std::memcpy(&value, chain.request_buffer(slot).data(), sizeof(value));
            if (value != (op.key ^ UINT64_C(0xa5a5a5a5a5a5a5a5))) {
                std::fprintf(stderr, "SONIC PMCHAIN returned an incorrect payload for key=%" PRIu64 "\n", op.key);
                return EIO;
            }
        }
        return 0;
    }
};

static int query_batch(void *raw_context, uint64_t first_query_id, uint64_t count) {
    // Return errors to the shared harness so it stops and joins its producer.
    try {
        return static_cast<RampContext *>(raw_context)->access_batch(first_query_id, count, false);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "SONIC PMCHAIN batch lookup failed: %s\n", error.what());
        return EIO;
    }
}

static int run_ramp(const ramp_latency_config &ramp, uint64_t batch_size, uint32_t threads) {
    const uint64_t n = ramp.map_size;
    sn::omap::pmchain::util::zingoram_config_input config{};
    config.block_count = n;
    config.batch_size = batch_size;
    // Same fixed settings as the artifact's PMCHAIN experiment.
    config.bucket_real_size = 14;
    config.bucket_dummy_size = 10;
    config.routing_depth = 5;
    config.evict_batch = 4;
    config.access_concurrency = threads;
    const auto setup = sn::omap::pmchain::util::compute_zingoram_setup<Traits>(config);
    sn::threads::pthread_thread_pool pool(threads - 1);
    Backing backing(setup.opts, sn::threads::thread_team(pool.pool(), threads));
    Adapter adapter(backing, {.block_count = n, .disjoint_epoch_window = setup.disjoint_window});
    Posmap posmap({.block_count = batch_size, .bucket_size = 64},
                  sn::threads::thread_team(pool.pool(), threads));
    sn::omap::pmchain::config chain_config{};
    chain_config.block_count = n;
    chain_config.oram_block_bytes = sizeof(uint64_t);
    chain_config.batch_size = batch_size;
    chain_config.oram_parallelism = threads;
    chain_config.drop_epoch = false;
    Chain chain(chain_config, posmap, adapter, sn::threads::thread_team(pool.pool(), threads));
    chain.initialize();
    RampContext context{chain, backing, n, std::vector<Chain::operation>(batch_size)};

    std::fprintf(stdout,
        "SONIC_RAMP threads=%u key_bytes=8 value_bytes=8 populated_keys=%" PRIu64
        " physical_batch_size=%" PRIu64 " disjoint_epoch_window=%" PRIu64 "\n",
        threads, n, batch_size, setup.disjoint_window);
    std::fflush(stdout);
    // Preload the entire map, outside calibration and measured arrivals. The
    // final write batch is padded too when N is not a multiple of B.
    for (uint64_t first = 0; first < n; first += batch_size) {
        const int status = context.access_batch(first, std::min(batch_size, n - first), true);
        if (status != 0) return status;
    }

    double calibration_qps = 0.0;
    int status = ramp_latency_calibrate_batch(&context, query_batch, ramp.calibration_queries,
                                              ramp.caller_max_batch_size, &calibration_qps);
    if (status != 0) {
        std::fprintf(stderr, "SONIC PMCHAIN calibration failed: %d\n", status);
        return status;
    }
    ramp_latency_print_calibration(&ramp, calibration_qps);
    ramp_latency_summary summary;
    status = ramp_latency_run_batch(&ramp, calibration_qps, &context, query_batch, &summary);
    if (status != 0) {
        std::fprintf(stderr, "SONIC PMCHAIN ramp benchmark failed: %d\n", status);
        return status;
    }
    ramp_latency_print_summary(&ramp, &summary);
    return 0;
}

int main(int argc, char **argv) {
    uint64_t n = 65536;
    if (argc > 3) {
        std::fprintf(stderr, "usage: %s [N [output.csv]]\n", argv[0]);
        return EINVAL;
    }
    if (argc > 1) {
        const char *end = argv[1] + std::strlen(argv[1]);
        const auto parsed = std::from_chars(argv[1], end, n);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            std::fprintf(stderr, "map size must be an integer\n");
            return EINVAL;
        }
    }
    // R=5, E=4 need a non-empty eviction subpath; direct_block uses 32-bit addresses.
    if (n < 2048 || n > UINT32_MAX) {
        std::fprintf(stderr, "map size must be between 2048 and %u\n", UINT32_MAX);
        return EINVAL;
    }
    const std::string output = argc > 2 ? argv[2] : "ramp_latency_sonic_pmchain_N" + std::to_string(n) + ".csv";
    ramp_latency_config config;
    ramp_latency_default_config(&config, n, "sonic_pmchain", output.c_str());
    ramp_latency_enable_batching(&config, std::min<uint64_t>(65536, std::bit_floor(n)));
    if (config.caller_max_batch_size > n) {
        std::fprintf(stderr, "RAMP_MAX_BATCH_SIZE must not exceed N\n");
        return EINVAL;
    }
    // O2TH needs a multiple of its bucket size. Use a power of two to match the
    // PMCHAIN sweep; the caller cap may still be arbitrary (including one).
    const uint64_t batch_size = std::bit_ceil(std::max<uint64_t>(64, config.caller_max_batch_size));
    if (batch_size > n) {
        std::fprintf(stderr, "RAMP_MAX_BATCH_SIZE rounded up to a power of two must fit N\n");
        return EINVAL;
    }
    config.check_output = 1;
    ramp_latency_print_config(&config);
    try {
        return run_ramp(config, batch_size, 8);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "SONIC PMCHAIN ramp failed: %s\n", error.what());
        return EIO;
    }
}
