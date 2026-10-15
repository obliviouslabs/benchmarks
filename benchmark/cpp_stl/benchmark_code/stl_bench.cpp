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

// Compiler barriers, not CPU fences. Register operands force actual loads of
// every queried payload word; a memory-only operand on a reference need not.
inline void do_not_optimize(uint64_t value) {
    asm volatile("" : : "r"(value) : "memory");
}

inline void clobber_memory() {
    asm volatile("" : : : "memory");
}

template <class Action>
uint64_t time_ns(Action action) {
    clobber_memory();
    const uint64_t start = current_time_ns();
    clobber_memory();
    action();
    clobber_memory();
    const uint64_t end = current_time_ns();
    clobber_memory();
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
template <size_t ValueBytes>
[[gnu::noinline]] void insert_array(Value<ValueBytes> *array, const uint64_t *keys,
                                  const Value<ValueBytes> *values, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        array[keys[i]] = values[i];
        clobber_memory(); // Do not coalesce/remove the individual stores.
    }
}

template <size_t ValueBytes>
[[gnu::noinline]] QueryResult query_array(const Value<ValueBytes> *array,
                                        const uint64_t *keys, size_t n) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto value = array[keys[i]];
        for (uint64_t word : value) do_not_optimize(word);
        checksum += value[0];
    }
    return {checksum, true};
}

template <size_t ValueBytes>
using Map = std::unordered_map<uint64_t, Value<ValueBytes>>;

template <size_t ValueBytes>
[[gnu::noinline]] bool insert_map(Map<ValueBytes> &map, const uint64_t *keys,
                                const Value<ValueBytes> *values, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const auto inserted = map.emplace(keys[i], values[i]);
        if (!inserted.second) return false;
        clobber_memory();
    }
    return true;
}

template <size_t ValueBytes>
[[gnu::noinline]] QueryResult query_map(const Map<ValueBytes> &map,
                                      const uint64_t *keys, size_t n) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto found = map.find(keys[i]);
        if (found == map.end()) return {checksum, false};
        const auto value = found->second;
        for (uint64_t word : value) do_not_optimize(word);
        checksum += value[0];
    }
    return {checksum, true};
}

struct Measurements {
    uint64_t initialization_ns = 0;
    uint64_t insertion_ns = 0;
    uint64_t query_ns = 0;
    uint64_t memory_kb = 0;
};

void report(bool map, size_t n, size_t value_bytes, const Measurements &m,
            size_t buckets = 0, float max_load_factor = 0, float load_factor = 0) {
    const char *query = map ? "Get" : "Read";
    // Use fprintf directly: the shared REPORT_LINE buffer is only 1024 bytes.
    std::fprintf(stderr,
        "REPORT_123%s|cpp_stl|%s| N := %zu | Key_bytes := 8 | Value_bytes := %zu"
        " | Threads := 1 | Insertions := %zu | Queries := %zu"
        " | Workload := full_population_shuffled_hits"
        " | Initialization_time_us := %.6f"
        " | Insertion_latency_us := %.6f | Insertion_throughput_qps := %.2f"
        " | %s_latency_us := %.6f | %s_throughput_qps := %.2f"
        " | Memory_kb := %" PRIu64,
        map ? "UnorderedMap" : "RORAM", version_string(), n, value_bytes, n, n,
        m.initialization_ns / 1000.0,
        m.insertion_ns / (1000.0 * n), n * 1e9 / m.insertion_ns,
        query, m.query_ns / (1000.0 * n), query, n * 1e9 / m.query_ns, m.memory_kb);
    if (map) {
        std::fprintf(stderr,
            " | Max_load_factor := %.9f | Load_factor := %.9f | Bucket_count := %zu",
            static_cast<double>(max_load_factor), static_cast<double>(load_factor), buckets);
    } else {
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

template <size_t ValueBytes>
int benchmark_array(size_t n) try {
    Inputs<ValueBytes> inputs(n, false); // All key/payload generation is untimed.
    const uint64_t memory_before = getMemValue();
    std::vector<Value<ValueBytes>> array;
    Measurements m;
    m.initialization_ns = time_ns([&] { array.resize(n); }); // Allocate and zero.
    m.insertion_ns = time_ns([&] {
        insert_array<ValueBytes>(array.data(), inputs.keys.data(), inputs.values.data(), n);
    });
    inputs.prepare_queries(); // Shuffle outside both timed loops.
    QueryResult result{};
    m.query_ns = time_ns([&] { result = query_array<ValueBytes>(array.data(), inputs.keys.data(), n); });
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
    report(false, n, ValueBytes, m);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "STL array failed: %s\n", error.what());
    return 1;
}

template <size_t ValueBytes>
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
        inserted = insert_map<ValueBytes>(map, inputs.keys.data(), inputs.values.data(), n);
    });
    if (!inserted || map.size() != n || map.bucket_count() != reserved_buckets) {
        std::fprintf(stderr, "STL map insertion count or reserved bucket count mismatch\n");
        return 1;
    }
    inputs.prepare_queries();
    QueryResult result{};
    m.query_ns = time_ns([&] { result = query_map<ValueBytes>(map, inputs.keys.data(), n); });
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
    report(true, n, ValueBytes, m, map.bucket_count(), map.max_load_factor(), map.load_factor());
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "STL map failed: %s\n", error.what());
    return 1;
}

int main() {
    constexpr float max_load_factor = 1.0f;
    // Work is exactly N insertions followed by N queries per case. Large cases
    // also need substantial untimed input preparation and validation; use selectors.
    for (unsigned exponent = 10; exponent <= 28; ++exponent) {
        const size_t n = size_t{1} << exponent;
        RUN_TEST_FORKED(benchmark_array<8>(n),
            "N=%zu,key_bytes=8,value_bytes=8,threads=1", n);
        RUN_TEST_FORKED(benchmark_array<56>(n),
            "N=%zu,key_bytes=8,value_bytes=56,threads=1", n);
        RUN_TEST_FORKED(benchmark_unordered_map<8>(n, max_load_factor),
            "N=%zu,key_bytes=8,value_bytes=8,threads=1,max_load_factor=%.3g", n,
            static_cast<double>(max_load_factor));
        RUN_TEST_FORKED(benchmark_unordered_map<56>(n, max_load_factor),
            "N=%zu,key_bytes=8,value_bytes=56,threads=1,max_load_factor=%.3g", n,
            static_cast<double>(max_load_factor));
    }
    return 0;
}
