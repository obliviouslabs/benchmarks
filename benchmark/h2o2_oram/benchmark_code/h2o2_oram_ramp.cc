#include <cerrno>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "hash_planner.hpp"
#include "oram.hpp"
#include "ramp_latency.h"
#include "types.hpp"

using IndexType = std::size_t;
using ValueType = ORAM::Block<IndexType, 64 - sizeof(IndexType)>;
using RampOram = ORAM::ObliviousRAM<IndexType, ValueType>;

struct RampContext {
    RampOram *oram;
    std::uint64_t size;
};

static int query_oram(void *raw_context, std::uint64_t query_id) {
    RampContext *context = static_cast<RampContext *>(raw_context);
    const IndexType index = static_cast<IndexType>(query_id % context->size);
    const ValueType &value = (*context->oram)[index];

    if (value.id != index) {
        std::fprintf(stderr,
                     "h2o2_oram lookup produced an incorrect result for "
                     "query_id=%" PRIu64 ": index=%zu, value.id=%zu\n",
                     query_id, index, value.id);
        return EIO;
    }
    return 0;
}

int main(int argc, char **argv) {
    const std::uint64_t n =
        argc > 1 ? std::strtoull(argv[1], nullptr, 10) : UINT64_C(65536);
    const std::string output =
        argc > 2 ? argv[2]
                 : "ramp_latency_h2o2_oram_N" + std::to_string(n) + ".csv";
    if (n == 0 || n > std::numeric_limits<IndexType>::max()) {
        std::fprintf(stderr, "map size must be between 1 and %zu\n",
                     std::numeric_limits<IndexType>::max());
        return EINVAL;
    }

    std::vector<ValueType> raw_data(static_cast<IndexType>(n));
    for (IndexType index = 0; index < raw_data.size(); ++index)
        raw_data[index].id = index;
    RampOram oram(raw_data.begin(), raw_data.end());

    ramp_latency_config config;
    ramp_latency_default_config(&config, n, "h2o2_oram", output.c_str());
    RampContext context{&oram, n};

    double calibration_qps = 0.0;
    int status = ramp_latency_calibrate(
        &context, query_oram, config.calibration_queries, &calibration_qps);
    if (status != 0) {
        std::fprintf(stderr, "calibration failed: %d\n", status);
        return status;
    }

    ramp_latency_summary summary;
    status = ramp_latency_run(&config, calibration_qps, &context, query_oram,
                              &summary);
    if (status != 0) {
        std::fprintf(stderr, "ramp benchmark failed: %d\n", status);
        return status;
    }
    ramp_latency_print_summary(&config, &summary);
    return 0;
}
