#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include "odsl/par_omap.hpp"
#include "ramp_latency.h"

static constexpr uint64_t kShardCount = 15;
static constexpr uint64_t kDefaultMaximumBatchSize = 65536;

using RampBatchMap = ODSL::ParOMap<uint64_t, uint64_t, uint32_t>;

struct RampBatchContext {
    RampBatchMap *map;
    uint64_t map_size;
    bool check_output;
    std::vector<uint64_t> keys;
    std::vector<uint64_t> values;
};

static bool output_checking_enabled() {
    const char *raw = std::getenv("RAMP_CHECK_OUTPUT");
    if (raw == nullptr)
        return false;
    const std::string value(raw);
    return !value.empty() && value != "0" && value != "false";
}

static int query_map_batch(void *raw_context, uint64_t first_query_id,
                           uint64_t query_count) {
    RampBatchContext *context = static_cast<RampBatchContext *>(raw_context);
    if (query_count == 0 || query_count > context->keys.size())
        return EINVAL;

    for (uint64_t offset = 0; offset < query_count; ++offset)
        context->keys[offset] = (first_query_id + offset) % context->map_size;

    try {
        auto found = context->map->FindBatch(
            context->keys.begin(), context->keys.begin() + query_count,
            context->values.begin());
        if (found.size() != query_count) {
            std::fprintf(stderr,
                         "olabs_oram batch lookup returned %zu flags for %" PRIu64
                         " requests\n",
                         found.size(), query_count);
            return EIO;
        }
        if (context->check_output) {
            for (uint64_t offset = 0; offset < query_count; ++offset) {
                if (found[offset] == 0 || context->values[offset] != context->keys[offset]) {
                    std::fprintf(stderr,
                                 "olabs_oram batch lookup produced an incorrect result for "
                                 "query_id=%" PRIu64 ": key=%" PRIu64 ", found=%u, value=%" PRIu64
                                 "\n",
                                 first_query_id + offset, context->keys[offset],
                                 static_cast<unsigned>(found[offset]), context->values[offset]);
                    return EIO;
                }
            }
        }
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "olabs_oram batch lookup failed: %s\n", error.what());
        return EIO;
    }
}

int main(int argc, char **argv) {
    uint64_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1024;
    std::string output = argc > 2
                             ? argv[2]
                             : "ramp_latency_olabs_oram_sharded_N" +
                                   std::to_string(n) + ".csv";
    if (n == 0) {
        std::fprintf(stderr, "map size must be greater than zero\n");
        return EINVAL;
    }

    const uint64_t capacity = n * 5 / 4;
    RampBatchMap map(capacity, kShardCount);
    RampBatchMap::InitContext *init =
        map.NewInitContext(n, MAX_CACHE_SIZE);
    for (uint64_t key = 0; key < n; ++key)
        init->Insert(key, key);
    init->Finalize();
    delete init;

    ramp_latency_config config;
    ramp_latency_default_config(&config, n, "olabs_oram_sharded", output.c_str());
    ramp_latency_enable_batching(&config, kDefaultMaximumBatchSize);
    if (config.caller_max_batch_size > UINT32_MAX) {
        std::fprintf(stderr, "RAMP_MAX_BATCH_SIZE must not exceed %u\n", UINT32_MAX);
        return EINVAL;
    }
    const bool check_output = output_checking_enabled();
    config.check_output = check_output ? 1 : 0;
    if (check_output)
        std::fprintf(stderr,
                     "RAMP_CHECK_OUTPUT enabled: checking every batch result\n");

    RampBatchContext context{
        &map,
        n,
        check_output,
        std::vector<uint64_t>(config.caller_max_batch_size),
        std::vector<uint64_t>(config.caller_max_batch_size),
    };

    double calibration_qps = 0.0;
    int status = ramp_latency_calibrate_batch(
        &context, query_map_batch, config.calibration_queries,
        config.caller_max_batch_size, &calibration_qps);
    if (status != 0) {
        std::fprintf(stderr, "batch calibration failed: %d\n", status);
        return status;
    }

    ramp_latency_summary summary;
    status = ramp_latency_run_batch(&config, calibration_qps, &context,
                                    query_map_batch, &summary);
    if (status != 0) {
        std::fprintf(stderr, "batch ramp benchmark failed: %d\n", status);
        return status;
    }
    ramp_latency_print_summary(&config, &summary);
    return 0;
}
