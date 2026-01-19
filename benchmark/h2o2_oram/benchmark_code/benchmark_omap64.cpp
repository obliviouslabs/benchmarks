
#include <benchmark/benchmark.h>
#include <vector>
#include "hash_planner.hpp"
#include "omap.hpp"
#include "types.hpp"

template <std::size_t SIZE>
struct SizedBlock
{
    uint8_t value[SIZE];
    SizedBlock() : value{0} {}
    bool operator==(const SizedBlock<SIZE> &other) const
    {
        bool ret = true;
        for (size_t i = 0; i < SIZE; i++)
            ret &= (value[i] == other.value[i]);
        return ret;
    }

    // Constructor from any arithmetic type that fits into the value array
    template <typename T>
        requires(sizeof(T) == SIZE)
    SizedBlock(T input)
    {
        std::memcpy(value, &input, sizeof(T)); // Copy new data
    }

    // Template function to get the value as a basic type
    template <typename T>
        requires(sizeof(T) == SIZE && std::is_standard_layout_v<T>)
    explicit operator T() const
    {
        T output;
        std::memcpy(&output, this, sizeof(T));
        return output;
    }
};

class OMapDataFixture64 : public benchmark::Fixture
{
public:
    size_t n;
    using IndexType = size_t;
    std::vector<std::pair<IndexType, SizedBlock<56>>> raw_data;
    std::random_device rd;
    ORAM::ObliviousMap<IndexType, SizedBlock<56>> omap;
    void SetUp(const ::benchmark::State &state) override
    {
        std::mt19937 gen(rd());
        n = state.range(0);
        raw_data.resize(n);
        std::uniform_int_distribution<> dist(0, std::numeric_limits<int>::max());
        for (size_t i = 0; i < n; i++) {
            raw_data[i].first = dist(gen);
            raw_data[i].second.value[0] = static_cast<uint8_t>(dist(gen) % 256);
            raw_data[i].second.value[55] = static_cast<uint8_t>(dist(gen) % 256);
        }
        std::shuffle(raw_data.begin(), raw_data.end(), gen);
    }

    void TearDown(const ::benchmark::State &) override
    {
        raw_data.clear();
        raw_data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(OMapDataFixture64, OMap)
(benchmark::State &state)
{
    for (auto _ : state)
    {
        for (uint32_t i = 0; i < n; i++)
            omap.insert(raw_data[i].first, raw_data[i].second);
        omap.insert(raw_data[0].first, raw_data[0].second);
    }
}

static void CustomizedArgsN(benchmark::internal::Benchmark *b)
{
    for (size_t i = 8; i <= 31; i++) // n := 2**i
    {
        size_t n = 1ll << i;
        b->Args({(int64_t)n});
    }
}

BENCHMARK_REGISTER_F(OMapDataFixture64, OMap)->Apply(CustomizedArgsN)->MeasureProcessCPUTime()->UseRealTime();