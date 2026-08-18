#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "odsl/omap.hpp"
#include "ramp_latency.h"

using RampMap = ODSL::OMap<uint64_t, uint64_t, uint32_t>;

static int query_map(void *context, uint64_t query_id) {
    (void)query_id;
    RampMap *map = static_cast<RampMap *>(context);
    uint64_t value = 0;
    map->Find(0, value);
    return 0;
}

int main(int argc, char **argv) {
    uint64_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1024;
    std::string output =
        argc > 2 ? argv[2] : "ramp_latency_olabs_oram_N" + std::to_string(n) + ".csv";
    if (n == 0) {
        std::fprintf(stderr, "map size must be greater than zero\n");
        return EINVAL;
    }

    const uint64_t capacity = n * 5 / 4;
    RampMap map(capacity);
    RampMap::InitContext *init = map.NewInitContext(MAX_CACHE_SIZE);
    for (uint64_t key = 0; key < n; ++key)
        init->Insert(key, key);
    init->Finalize();
    delete init;

    ramp_latency_config config;
    ramp_latency_default_config(&config, n, "olabs_oram", output.c_str());

    double calibration_qps = 0.0;
    int status =
        ramp_latency_calibrate(&map, query_map, config.calibration_queries, &calibration_qps);
    if (status != 0) {
        std::fprintf(stderr, "calibration failed: %d\n", status);
        return status;
    }

    ramp_latency_summary summary;
    status = ramp_latency_run(&config, calibration_qps, &map, query_map, &summary);
    if (status != 0) {
        std::fprintf(stderr, "ramp benchmark failed: %d\n", status);
        return status;
    }
    ramp_latency_print_summary(&config, &summary);
    return 0;
}
