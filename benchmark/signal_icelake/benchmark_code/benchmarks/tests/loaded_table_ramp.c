// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>

#include "ohtable/ohtable.h"
#include "ramp_latency.h"
#include "util/tests.h"

typedef struct ramp_record_8_8 {
    u64 key;
    u64 value;
} ramp_record_8_8;

static int query_table(void *context, uint64_t query_id) {
    (void)query_id;
    ramp_record_8_8 recovered;
    return ohtable_get((ohtable *)context, 0, (uint64_t *)&recovered);
}

int main(int argc, char **argv) {
    uint64_t n = argc > 1 ? strtoull(argv[1], NULL, 10) : 1024;
    char default_output[128];
    const char *output;
    if (n == 0) {
        fprintf(stderr, "map size must be greater than zero\n");
        return EINVAL;
    }
    snprintf(default_output, sizeof(default_output), "ramp_latency_signal_icelake_N%" PRIu64 ".csv",
             n);
    output = argc > 2 ? argv[2] : default_output;

    size_t capacity = (size_t)(n * 5 / 4);
    ohtable *table = ohtable_create(capacity, 2, TEST_STASH_SIZE, getentropy);
    if (table == NULL)
        return ENOMEM;
    for (uint64_t key = 0; key < n; ++key) {
        ramp_record_8_8 record = {.key = key, .value = key};
        int status = ohtable_put(table, (const uint64_t *)&record);
        if (status != err_SUCCESS) {
            ohtable_destroy(table);
            return status;
        }
    }

    ramp_latency_config config;
    ramp_latency_default_config(&config, n, "signal_icelake", output);
    double calibration_qps = 0.0;
    int status =
        ramp_latency_calibrate(table, query_table, config.calibration_queries, &calibration_qps);
    if (status == 0) {
        ramp_latency_summary summary;
        status = ramp_latency_run(&config, calibration_qps, table, query_table, &summary);
        if (status == 0)
            ramp_latency_print_summary(&config, &summary);
    }

    ohtable_destroy(table);
    if (status != 0)
        fprintf(stderr, "ramp benchmark failed: %d\n", status);
    return status;
}
