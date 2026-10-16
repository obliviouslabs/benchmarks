#include "common.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <random>
#include <unordered_map>
#include <vector>

// Compiler-only anti-elision marker. Register operands force actual loads of
// every queried payload word; a memory-only operand on a reference need not.
inline void do_not_optimize(uint64_t value) {
    asm volatile("" : : "r"(value) : "memory");
}

// Full instruction-stream serialization, not merely compiler/memory ordering.
// CPUID also completes prior stores before the next iteration starts. Its cost
// is deliberately included in the measured time (can dominate in very small arrays/maps).
inline void serialize_cpu() {
#if defined(__x86_64__)
    uint32_t eax = 0, ebx, ecx = 0, edx; // Fixed, unprivileged leaf 0.
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx) : : "memory");
#else
#error "The serialized C++ STL benchmark requires x86-64 (CPUID)."
#endif
}

template <bool Serialized>
inline void iteration_barrier() {
    if constexpr (Serialized) {
        serialize_cpu();
    } else {
        // Compiler-only barrier: retain individual stores, but permit CPU overlap.
        asm volatile("" : : : "memory");
    }
}

template <class Action>
uint64_t time_ns(Action action) {
    // Identical clock boundaries in both modes; no per-operation clock calls.
    serialize_cpu();
    const uint64_t start = current_time_ns();
    serialize_cpu();
    action();
    serialize_cpu();
    const uint64_t end = current_time_ns();
    serialize_cpu();
    return end - start;
}

template <size_t ValueBytes>
using Value = std::array<uint64_t, ValueBytes / sizeof(uint64_t)>;

template <size_t ValueBytes>
Value<ValueBytes> payload(uint64_t key) {
    static_assert(ValueBytes > 0 && ValueBytes % sizeof(uint64_t) == 0);
    Value<ValueBytes> value;
    for (size_t word = 0; word < value.size(); ++word)
        value[word] = key ^ (UINT64_C(0x9e3779b97f4a7c15) * (word + 1));
    return value;
}

// An invertible permutation of uint64_t, so all N keys are distinct. Ordinary
// std::hash<uint64_t> is often the identity: consecutive keys would then have
// artificially few bucket collisions after reserve(N) at a load factor < 1.
uint64_t permute_key(uint64_t key) {
    key = (key ^ (key >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    key = (key ^ (key >> 27)) * UINT64_C(0x94d049bb133111eb);
    return key ^ (key >> 31);
}

template <size_t ValueBytes>
struct Inputs {
    std::vector<uint64_t> keys;
    std::vector<Value<ValueBytes>> values;
    std::mt19937_64 random{UINT64_C(0x123456789abcdef0)};
    uint64_t expected_checksum = 0;

    Inputs(size_t n, bool map_keys) : keys(n), values(n) {
        for (size_t i = 0; i < n; ++i)
            keys[i] = map_keys ? permute_key(i) : i;
        std::shuffle(keys.begin(), keys.end(), random);
        for (size_t i = 0; i < n; ++i) {
            values[i] = payload<ValueBytes>(keys[i]);
            expected_checksum += values[i][0]; // Defined uint64_t wraparound.
        }
    }

    void prepare_queries() {
        // A fresh permutation of all N populated keys, without extra storage.
        std::shuffle(keys.begin(), keys.end(), random);
    }
};

struct QueryResult {
    uint64_t checksum;
    bool found;
};

// Keep the timed kernels identifiable in optimized disassembly. These noinline
// wrappers add one call per phase, not per access; STL internals may still call out.
template <size_t ValueBytes, bool Serialized>
[[gnu::noinline]] void insert_array(Value<ValueBytes> *array, const uint64_t *keys,
                                  const Value<ValueBytes> *values, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        array[keys[i]] = values[i];
        iteration_barrier<Serialized>();
    }
}

template <size_t ValueBytes, bool Serialized>
[[gnu::noinline]] QueryResult query_array(const Value<ValueBytes> *array,
                                        const uint64_t *keys, size_t n) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto value = array[keys[i]];
        for (uint64_t word : value) do_not_optimize(word);
        checksum += value[0];
        do_not_optimize(checksum); // Keep register-only accumulation before the barrier too.
        iteration_barrier<Serialized>();
    }
    return {checksum, true};
}

template <size_t ValueBytes>
using Map = std::unordered_map<uint64_t, Value<ValueBytes>>;

template <size_t ValueBytes, bool Serialized>
[[gnu::noinline]] bool insert_map(Map<ValueBytes> &map, const uint64_t *keys,
                                const Value<ValueBytes> *values, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const auto inserted = map.emplace(keys[i], values[i]);
        if (!inserted.second) return false;
        iteration_barrier<Serialized>();
    }
    return true;
}

template <size_t ValueBytes, bool Serialized>
[[gnu::noinline]] QueryResult query_map(const Map<ValueBytes> &map,
                                      const uint64_t *keys, size_t n) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto found = map.find(keys[i]);
        if (found == map.end()) return {checksum, false};
        const auto value = found->second;
        for (uint64_t word : value) do_not_optimize(word);
        checksum += value[0];
        do_not_optimize(checksum);
        iteration_barrier<Serialized>();
    }
    return {checksum, true};
}

struct Measurements {
    uint64_t initialization_ns = 0;
    uint64_t insertion_ns = 0;
    uint64_t query_ns = 0;
    uint64_t memory_kb = 0;
};

template <bool Serialized>
void report(bool map, size_t n, size_t value_bytes, const Measurements &m,
            size_t buckets = 0, float max_load_factor = 0, float load_factor = 0) {
    const char *query = map ? "Get" : "Read";
    // Use fprintf directly: the shared REPORT_LINE buffer is only 1024 bytes.
    std::fprintf(stderr,
        "REPORT_123%s|cpp_stl|%s| N := %zu | Key_bytes := 8 | Value_bytes := %zu"
        " | Threads := 1 | Insertions := %zu | Queries := %zu"
        " | Workload := full_population_shuffled_hits"
        " | Timing_mode := %s | Iteration_barrier := %s",
        map ? "UnorderedMap" : "RORAM", version_string(), n, value_bytes, n, n,
        Serialized ? "serialized" : "pipelined", Serialized ? "cpuid" : "compiler");
    // Complementary metrics: never mix pipelined averages into latency tables,
    // or serialized throughput into throughput tables. No reciprocal backfill.
    if constexpr (Serialized) {
        std::fprintf(stderr,
            " | Insertion_latency_us := %.6f | %s_latency_us := %.6f"
            " | Serialized_initialization_time_us := %.6f | Serialized_memory_kb := %" PRIu64,
            m.insertion_ns / (1000.0 * n), query, m.query_ns / (1000.0 * n),
            m.initialization_ns / 1000.0, m.memory_kb);
    } else {
        std::fprintf(stderr,
            " | Insertion_throughput_qps := %.2f | %s_throughput_qps := %.2f"
            " | Initialization_time_us := %.6f | Memory_kb := %" PRIu64,
            n * 1e9 / m.insertion_ns, query, n * 1e9 / m.query_ns,
            m.initialization_ns / 1000.0, m.memory_kb);
    }
    if (map) {
        std::fprintf(stderr,
            " | Max_load_factor := %.9f | Load_factor := %.9f | Bucket_count := %zu",
            static_cast<double>(max_load_factor), static_cast<double>(load_factor), buckets);
    } else if constexpr (!Serialized) {
        std::fprintf(stderr, " | Initialization_zeroed_time_us := %.6f", m.initialization_ns / 1000.0);
    }
    char compiler[] = __VERSION__;
    sanitize(compiler);
    std::fprintf(stderr, " | Compiler := %s", compiler);
#if defined(_LIBCPP_VERSION)
    std::fprintf(stderr, " | Standard_library := libc++-%d", _LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
    std::fprintf(stderr, " | Standard_library := libstdc++-%d", __GLIBCXX__);
#endif
    std::fprintf(stderr, "REPORT_123\n");
}

template <size_t ValueBytes, bool Serialized>
int benchmark_array(size_t n) try {
    Inputs<ValueBytes> inputs(n, false); // All key/payload generation is untimed.
    const uint64_t memory_before = getMemValue();
    std::vector<Value<ValueBytes>> array;
    Measurements m;
    m.initialization_ns = time_ns([&] { array.resize(n); }); // Allocate and zero.
    m.insertion_ns = time_ns([&] {
        insert_array<ValueBytes, Serialized>(array.data(), inputs.keys.data(), inputs.values.data(), n);
    });
    inputs.prepare_queries(); // Shuffle outside both timed loops.
    QueryResult result{};
    m.query_ns = time_ns([&] {
        result = query_array<ValueBytes, Serialized>(array.data(), inputs.keys.data(), n);
    });
    const uint64_t memory_after = getMemValue();
    m.memory_kb = memory_after > memory_before ? memory_after - memory_before : 0;
    if (result.checksum != inputs.expected_checksum) {
        std::fprintf(stderr, "STL array query checksum mismatch\n");
        return 1;
    }
    // Validate every stored word outside timing, separately from the N timed reads.
    for (size_t key = 0; key < n; ++key) {
        if (array[key] != payload<ValueBytes>(key)) {
            std::fprintf(stderr, "STL array payload mismatch at key=%zu\n", key);
            return 1;
        }
    }
    report<Serialized>(false, n, ValueBytes, m);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "STL array failed: %s\n", error.what());
    return 1;
}

template <size_t ValueBytes, bool Serialized>
int benchmark_unordered_map(size_t n, float max_load_factor) try {
    Inputs<ValueBytes> inputs(n, true);
    const uint64_t memory_before = getMemValue();
    Map<ValueBytes> map;
    Measurements m;
    m.initialization_ns = time_ns([&] {
        map.max_load_factor(max_load_factor); // Set BEFORE reserve, which sizes buckets from it.
        map.reserve(n);
    });
    const size_t reserved_buckets = map.bucket_count();
    bool inserted = false;
    m.insertion_ns = time_ns([&] {
        inserted = insert_map<ValueBytes, Serialized>(map, inputs.keys.data(), inputs.values.data(), n);
    });
    if (!inserted || map.size() != n || map.bucket_count() != reserved_buckets) {
        std::fprintf(stderr, "STL map insertion count or reserved bucket count mismatch\n");
        return 1;
    }
    inputs.prepare_queries();
    QueryResult result{};
    m.query_ns = time_ns([&] { result = query_map<ValueBytes, Serialized>(map, inputs.keys.data(), n); });
    const uint64_t memory_after = getMemValue();
    m.memory_kb = memory_after > memory_before ? memory_after - memory_before : 0;
    if (!result.found || result.checksum != inputs.expected_checksum) {
        std::fprintf(stderr, "STL map missing key or query checksum mismatch\n");
        return 1;
    }
    for (const auto &[key, value] : map) {
        if (value != payload<ValueBytes>(key)) {
            std::fprintf(stderr, "STL map payload mismatch at key=%" PRIu64 "\n", key);
            return 1;
        }
    }
    report<Serialized>(true, n, ValueBytes, m, map.bucket_count(), map.max_load_factor(), map.load_factor());
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "STL map failed: %s\n", error.what());
    return 1;
}

int main() {
    constexpr float max_load_factor = 1.0f;
    // Run each workload twice on fresh containers with identical inputs: first
    // pipelined throughput, then serialized latency. Each mode does N insertions
    // and N queries, with its own selector identity, timeout and correctness checks.
    // Large cases also need substantial untimed preparation/validation; use selectors.
    for (unsigned exponent = 10; exponent <= 28; ++exponent) {
        const size_t n = size_t{1} << exponent;
        RUN_TEST_FORKED((benchmark_array<8, false>(n)),
            "N=%zu,key_bytes=8,value_bytes=8,threads=1,timing_mode=pipelined,iteration_barrier=compiler", n);
        RUN_TEST_FORKED((benchmark_array<8, true>(n)),
            "N=%zu,key_bytes=8,value_bytes=8,threads=1,timing_mode=serialized,iteration_barrier=cpuid", n);
        RUN_TEST_FORKED((benchmark_array<56, false>(n)),
            "N=%zu,key_bytes=8,value_bytes=56,threads=1,timing_mode=pipelined,iteration_barrier=compiler", n);
        RUN_TEST_FORKED((benchmark_array<56, true>(n)),
            "N=%zu,key_bytes=8,value_bytes=56,threads=1,timing_mode=serialized,iteration_barrier=cpuid", n);
        RUN_TEST_FORKED((benchmark_unordered_map<8, false>(n, max_load_factor)),
            "N=%zu,key_bytes=8,value_bytes=8,threads=1,max_load_factor=%.3g,timing_mode=pipelined,iteration_barrier=compiler", n,
            static_cast<double>(max_load_factor));
        RUN_TEST_FORKED((benchmark_unordered_map<8, true>(n, max_load_factor)),
            "N=%zu,key_bytes=8,value_bytes=8,threads=1,max_load_factor=%.3g,timing_mode=serialized,iteration_barrier=cpuid", n,
            static_cast<double>(max_load_factor));
        RUN_TEST_FORKED((benchmark_unordered_map<56, false>(n, max_load_factor)),
            "N=%zu,key_bytes=8,value_bytes=56,threads=1,max_load_factor=%.3g,timing_mode=pipelined,iteration_barrier=compiler", n,
            static_cast<double>(max_load_factor));
        RUN_TEST_FORKED((benchmark_unordered_map<56, true>(n, max_load_factor)),
            "N=%zu,key_bytes=8,value_bytes=56,threads=1,max_load_factor=%.3g,timing_mode=serialized,iteration_barrier=cpuid", n,
            static_cast<double>(max_load_factor));
    }
    return 0;
}
