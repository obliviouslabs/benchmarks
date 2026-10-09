#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "common.h"
#include "sonic/crypto/buffered_prng.hpp"
#include "sonic/oram/zingoram/client.hpp"
#include "sonic/threads/platform/pthread_thread_pool.hpp"
#include "sonic/threads/thread_team.hpp"

struct Configuration {
    const char *name;
    const char *implementation;
    uint32_t threads;
    uint32_t real_blocks;
    uint32_t dummy_blocks;
    uint32_t evict_batch;
};

template <size_t ValueBytes>
int benchmark_nroram(uint64_t n, const Configuration &config) {
    constexpr uint64_t accesses = 2000000;
    using Traits = sn::oram::zingoram::traits<ValueBytes>;
    using Client = sn::oram::zingoram::client<Traits>;
    const uint64_t memory_before = getMemValue();
    const uint64_t init_start = current_time_ns();

    sn::oram::zingoram::options options{};
    options.block_count = n;
    options.bucket_real_size = config.real_blocks;
    options.bucket_dummy_size = config.dummy_blocks;
    options.routing_depth = 3;
    options.evict_batch = config.evict_batch;
    options.access_concurrency = config.threads;
    // Leave eviction_rate and stash_bound at zero: upstream derives safe values.
    // The default epoch mode includes normal evictions, never online-only timing.
    // Eight logical eviction workers, including for Ser.
    sn::threads::pthread_thread_pool eviction_pool(8 - 1);
    sn::threads::pthread_thread_pool access_pool(config.threads - 1);
    sn::threads::thread_team access_team(access_pool.pool(), config.threads);
    Client client(options, sn::threads::thread_team(eviction_pool.pool(), 8));
    client.initialize();
    const uint64_t init_end = current_time_ns();

    struct alignas(64) Worker {
        typename Client::access_scratch scratch;
        sn::crypto::buffered_prng<> random;
        alignas(64) std::array<uint8_t, ValueBytes> expected{};
        alignas(64) std::array<uint8_t, ValueBytes> output{};
        uint64_t leaf = 0;
        uint64_t latency_ns = 0;
        bool correct = true;
    };
    std::vector<Worker> workers(config.threads);
    const uint64_t leaf_count = client.shape().leaf_count;

    // Like the upstream standard experiment, use one fixed address per worker.
    // Caller-managed leaf positions make this NRORAM, not a recursive ORAM/map.
    // Write distinguishable payloads first so dropped/corrupted reads fail.
    for (uint32_t i = 0; i < config.threads; ++i) {
        auto &worker = workers[i];
        client.configure_access_scratch(worker.scratch);
        for (size_t b = 0; b < ValueBytes; ++b)
            worker.expected[b] = static_cast<uint8_t>((i + 1) * 31 + b);
        worker.leaf = worker.random.random_u64(0, leaf_count);
        sn::oram::access_request request{};
        request.address = i;
        request.cur_leaf = worker.random.random_u64(0, leaf_count);
        request.new_leaf = worker.leaf;
        request.is_write = true;
        request.in = {worker.expected.data(), ValueBytes};
        request.out = {worker.output.data(), ValueBytes};
        client.access(request, worker.scratch);
    }

    const uint64_t read_start = current_time_ns();
    access_team.parallel_work([&](size_t i) noexcept {
        auto &worker = workers[i];
        const uint64_t count = accesses / config.threads + (i < accesses % config.threads);
        sn::oram::access_request request{};
        request.address = static_cast<int64_t>(i);
        request.in = {worker.expected.data(), ValueBytes};
        request.out = {worker.output.data(), ValueBytes};
        for (uint64_t j = 0; j < count; ++j) {
            request.cur_leaf = worker.leaf;
            request.new_leaf = worker.random.random_u64(0, leaf_count);
            const uint64_t start = current_time_ns();
            client.access(request, worker.scratch);
            worker.latency_ns += current_time_ns() - start;
            worker.leaf = request.new_leaf;
            worker.correct &= worker.output == worker.expected;
        }
    });
    const uint64_t read_end = current_time_ns();
    const uint64_t memory_after = getMemValue();

    uint64_t latency_ns = 0;
    for (const auto &worker : workers) {
        if (!worker.correct) {
            std::fprintf(stderr, "SONIC returned an incorrect payload\n");
            return 1;
        }
        latency_ns += worker.latency_ns;
    }
    const uint64_t memory_kb = memory_after > memory_before ? memory_after - memory_before : 0;
    // Use the standard report framing with a distinct implementation per config.
    // Concurrent latency is measured per call, not inferred as 1 / throughput.
    std::fprintf(stderr,
        "REPORT_123NRORAM|%s|%s| N := %" PRIu64 " | Key_bytes := 8 | Value_bytes := %zu"
        " | Threads := %u | Eviction_threads := %u | Z := %u | S := %u | R := %u | E := %u"
        " | Accesses := %" PRIu64 " | Initialization_zeroed_time_us := %.2f"
        " | Read_latency_us := %.6f | Read_throughput_qps := %.2f | Memory_kb := %" PRIu64
        "REPORT_123\n",
        config.implementation, version_string(), n, ValueBytes, config.threads, 8U,
        options.bucket_real_size, options.bucket_dummy_size, options.routing_depth, options.evict_batch, accesses,
        (init_end - init_start) / 1000.0, latency_ns / (1000.0 * accesses),
        accesses * 1e9 / (read_end - read_start), memory_kb);
    return 0;
}

int main() {
    constexpr Configuration configurations[] = {
        {"Ser", "sonic_ser", 1, 8, 12, 1},
        {"Par8", "sonic_par8", 8, 16, 16, 2},
        {"Par16", "sonic_par16", 16, 16, 16, 2},
        {"Par32", "sonic_par32", 32, 14, 10, 4},
    };

    for (unsigned exponent = 10; exponent <= 28; ++exponent) {
        const uint64_t n = UINT64_C(1) << exponent;
        for (const auto &config : configurations) {
            RUN_TEST_FORKED(benchmark_nroram<8>(n, config),
                "N=%" PRIu64 ",value_bytes=8,config=%s,threads=%u,Z=%u,S=%u,R=3,E=%u",
                n, config.name, config.threads, config.real_blocks, config.dummy_blocks, config.evict_batch);
            RUN_TEST_FORKED(benchmark_nroram<56>(n, config),
                "N=%" PRIu64 ",value_bytes=56,config=%s,threads=%u,Z=%u,S=%u,R=3,E=%u",
                n, config.name, config.threads, config.real_blocks, config.dummy_blocks, config.evict_batch);
        }
    }
    return 0;
}
