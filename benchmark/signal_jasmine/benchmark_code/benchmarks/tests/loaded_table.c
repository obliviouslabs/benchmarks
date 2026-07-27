// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <stdio.h>
#include <sys/random.h>

#include "ohtable/ohtable.h"
#include "path_oram/path_oram.h"
#include "util/util.h"
#include "util/tests.h"
#include "common.h"

#ifndef SIGNAL_JASMINE_PATH_LENGTH
#define SIGNAL_JASMINE_PATH_LENGTH 0
#endif

typedef struct record_8_56
{
    u64 key;
    u64 a;
    u64 b;
    u64 c;
    u64 d;
    u64 e;
    u64 f;
} record_8_56;

typedef struct record_8_8
{
    u64 key;
    u64 a;
} record_8_8;



int loaded_table_8_56(size_t N)
{
    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();
    ohtable *ohtable = ohtable_create(7, getentropy);
    size_t cap = ohtable_capacity(ohtable);
    if (N > cap)
    {
        LOG_INFO("Skipping N=%zu; Jasmine table capacity is %zu records\n", N, cap);
        ohtable_destroy(ohtable);
        return err_SUCCESS;
    }

    size_t successfulGetCount = N;
    for (uint64_t i = 0; i < successfulGetCount; ++i)
    {
        record_8_56 r0 = {
            .key = i,
            .a = 2 * i,
            .b = 3 * i,
            .c = 4 * i,
            .d = 5 * i,
            .e = 6 * i,
            .f = 7 * i
        };
        RETURN_IF_ERROR(ohtable_put(ohtable, (const uint64_t *)&r0));
    }
    uint64_t end_ns_create = current_time_ns();


    // --- Time successful lookups ---
    uint64_t start_ns_success = current_time_ns();

    for (size_t i = 0; i < successfulGetCount; ++i)
    {
        record_8_56 recovered;
        RETURN_IF_ERROR(ohtable_get(ohtable, i, (uint64_t *)&recovered));
        TEST_ASSERT(i           == recovered.key);
        TEST_ASSERT(2 * i       == recovered.a);
        TEST_ASSERT(3 * i       == recovered.b);
        TEST_ASSERT(4 * i       == recovered.c);
        TEST_ASSERT(5 * i       == recovered.d);
        TEST_ASSERT(6 * i       == recovered.e);
        TEST_ASSERT(7 * i       == recovered.f);
    }

    uint64_t end_ns_success = current_time_ns();

    // --- Time failing lookups ---
    uint64_t start_ns_fail = current_time_ns();

    size_t failCount = successfulGetCount / 4;
    for (uint64_t i = (uint64_t)successfulGetCount; i < successfulGetCount + failCount; ++i)
    {
        record_8_56 recovered;
        RETURN_IF_ERROR(ohtable_get(ohtable, i, (uint64_t *)&recovered));
        TEST_ASSERT(UINT64_MAX  == recovered.key);
        TEST_ASSERT(UINT64_MAX  == recovered.a);
        TEST_ASSERT(UINT64_MAX  == recovered.b);
        TEST_ASSERT(UINT64_MAX  == recovered.c);
        TEST_ASSERT(UINT64_MAX  == recovered.d);
        TEST_ASSERT(UINT64_MAX  == recovered.e);
        TEST_ASSERT(UINT64_MAX  == recovered.f);
    }

    uint64_t end_ns_fail = current_time_ns();


    int memAfter = getMemValue();

    // Clean up
    ohtable_destroy(ohtable);

    // Compute and print average times
    double total_ns_success = (double)(end_ns_success - start_ns_success);
    double avg_ns_success   = total_ns_success / (double)successfulGetCount;

    double total_ns_fail    = (double)(end_ns_fail - start_ns_fail);
    double avg_ns_fail      = total_ns_fail / (double)failCount;

    double avg_ns_avg = (avg_ns_success + avg_ns_fail) / 2.0;

    REPORT_LINE("UnorderedMap", "Signal_Jasmine", "N:=%zu | Path_length := %d | Key_bytes := 8 | Value_bytes := 56 | Capacity_records := %zu | fill:=%.4f | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Memory_kb := %d",
                N, SIGNAL_JASMINE_PATH_LENGTH, cap, (double)successfulGetCount / (double)cap, (end_ns_create - start_ns_create) / 1000.0, avg_ns_avg / 1000.0, 1000000000.0 / avg_ns_avg, memAfter - memBefore);

    return err_SUCCESS;
}


int loaded_table_8_8(size_t N)
{
    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();
    ohtable *ohtable = ohtable_create(2, getentropy);
    size_t cap = ohtable_capacity(ohtable);
    if (N > cap)
    {
        LOG_INFO("Skipping N=%zu; Jasmine table capacity is %zu records\n", N, cap);
        ohtable_destroy(ohtable);
        return err_SUCCESS;
    }

    size_t successfulGetCount = N;
    for (uint64_t i = 0; i < successfulGetCount; ++i)
    {
        record_8_8 r0 = {
            .key = i,
            .a = 2 * i
        };
        RETURN_IF_ERROR(ohtable_put(ohtable, (const uint64_t *)&r0));
    }
    uint64_t end_ns_create = current_time_ns();


    // --- Time successful lookups ---
    uint64_t start_ns_success = current_time_ns();

    for (size_t i = 0; i < successfulGetCount; ++i)
    {
        record_8_8 recovered;
        RETURN_IF_ERROR(ohtable_get(ohtable, i, (uint64_t *)&recovered));
        TEST_ASSERT(i           == recovered.key);
        TEST_ASSERT(2 * i       == recovered.a);
    }

    uint64_t end_ns_success = current_time_ns();

    // --- Time failing lookups ---
    uint64_t start_ns_fail = current_time_ns();

    size_t failCount = successfulGetCount / 4;
    for (uint64_t i = (uint64_t)successfulGetCount; i < successfulGetCount + failCount; ++i)
    {
        record_8_8 recovered;
        RETURN_IF_ERROR(ohtable_get(ohtable, i, (uint64_t *)&recovered));
        TEST_ASSERT(UINT64_MAX  == recovered.key);
        TEST_ASSERT(UINT64_MAX  == recovered.a);
    }

    uint64_t end_ns_fail = current_time_ns();


    int memAfter = getMemValue();

    // Clean up
    ohtable_destroy(ohtable);

    // Compute and print average times
    double total_ns_success = (double)(end_ns_success - start_ns_success);
    double avg_ns_success   = total_ns_success / (double)successfulGetCount;

    double total_ns_fail    = (double)(end_ns_fail - start_ns_fail);
    double avg_ns_fail      = total_ns_fail / (double)failCount;

    double avg_ns_avg = (avg_ns_success + avg_ns_fail) / 2.0;

    REPORT_LINE("UnorderedMap", "Signal_Jasmine", "N:=%zu | Path_length := %d | Key_bytes := 8 | Value_bytes := 8 | Capacity_records := %zu | fill:=%.4f | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Memory_kb := %d",
                N, SIGNAL_JASMINE_PATH_LENGTH, cap, (double)successfulGetCount / (double)cap, (end_ns_create - start_ns_create) / 1000.0, avg_ns_avg / 1000.0, 1000000000.0 / avg_ns_avg, memAfter - memBefore);

    return err_SUCCESS;
}




int main()
{
    // We run the tests up to 40GB of memory usage.
    //
    // Should take 35h to run
    // for (uint64_t i = 10; i <= 28; i++) {
    //     RUN_TEST_FORKED(loaded_table_8_8(1<<i));
    // }

    // Should take 33h to run
    for (uint64_t i = 10; i <= 28; i++) {
        RUN_TEST_FORKED(loaded_table_8_56(1<<i));
    }
    
    return 0;
}
