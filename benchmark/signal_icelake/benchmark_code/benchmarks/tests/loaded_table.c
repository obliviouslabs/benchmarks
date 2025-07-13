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



typedef struct record_8_56
{
    u64 key;
    u64 a;
    u64 b;
    u64 c;
    u64 d;
    u64 e;
    u64 f;
    u64 g;
} record_8_56;

typedef struct record_8_8
{
    u64 key;
    u64 a;
} record_8_8;



int loaded_table_8_56(size_t N)
{
    // size_t cap = 1000000 * 5 / 4; // So it multiplies to get the 80%

    size_t cap = N * 5 / 4; // So it multiplies to get the 80%
    
    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();
    ohtable *ohtable = ohtable_create(cap, 8, TEST_STASH_SIZE, getentropy);

    // Populate the table with 80% of capacity
    size_t successfulGetCount = (size_t)(0.8 * cap);
    for (uint64_t i = 0; i < successfulGetCount; ++i)
    {
        record_8_56 r0 = {
            .key = i,
            .a = 2 * i,
            .b = 3 * i,
            .c = 4 * i,
            .d = 5 * i,
            .e = 6 * i,
            .f = 7 * i,
            .g = 8 * i
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
        TEST_ASSERT(8 * i       == recovered.g);
    }

    uint64_t end_ns_success = current_time_ns();

    // --- Time failing lookups ---
    uint64_t start_ns_fail = current_time_ns();

    for (uint64_t i = (uint64_t)successfulGetCount; i < cap; ++i)
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
        TEST_ASSERT(UINT64_MAX  == recovered.g);
    }

    uint64_t end_ns_fail = current_time_ns();


    int memAfter = getMemValue();

    // Clean up
    ohtable_destroy(ohtable);

    // Compute and print average times
    double total_ns_success = (double)(end_ns_success - start_ns_success);
    double avg_ns_success   = total_ns_success / (double)successfulGetCount;

    size_t failCount        = cap - successfulGetCount;
    double total_ns_fail    = (double)(end_ns_fail - start_ns_fail);
    double avg_ns_fail      = total_ns_fail / (double)failCount;

    double avg_ns_avg = (avg_ns_success + avg_ns_fail) / 2.0;

    REPORT_LINE("UnorderedMap", "Signal", "N:=%zu | Key_bytes := 8 | Value_bytes := 56 | fill:=0.8 | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Memory_kb := %d",
                N, (end_ns_create - start_ns_create) / 1000.0, avg_ns_avg / 1000.0, 1000000000.0 / avg_ns_avg, memAfter - memBefore);

    return err_SUCCESS;
}


int loaded_table_8_8(size_t N)
{
    // size_t cap = 1000000 * 5 / 4; // So it multiplies to get the 80%

    size_t cap = N * 5 / 4; // So it multiplies to get the 80%
    
    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();
    ohtable *ohtable = ohtable_create(cap, 2, TEST_STASH_SIZE, getentropy);

    // Populate the table with 80% of capacity
    size_t successfulGetCount = (size_t)(0.8 * cap);
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

    for (uint64_t i = (uint64_t)successfulGetCount; i < cap; ++i)
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

    size_t failCount        = cap - successfulGetCount;
    double total_ns_fail    = (double)(end_ns_fail - start_ns_fail);
    double avg_ns_fail      = total_ns_fail / (double)failCount;

    double avg_ns_avg = (avg_ns_success + avg_ns_fail) / 2.0;

    REPORT_LINE("UnorderedMap", "Signal", "N:=%zu | Key_bytes := 8 | Value_bytes := 8 | fill:=0.8 | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Memory_kb := %d",
                N, (end_ns_create - start_ns_create) / 1000.0, avg_ns_avg / 1000.0, 1000000000.0 / avg_ns_avg, memAfter - memBefore);

    return err_SUCCESS;
}




int main()
{
    // We run the tests up to 40GB of memory usage.
    //
    // Should take 35h to run
    for (uint64_t i = 10; i <= 26; i++) {
        RUN_TEST_FORKED(loaded_table_8_8(1<<i));
    }

    // Should take 33h to run
    for (uint64_t i = 10; i <= 26; i++) {
        RUN_TEST_FORKED(loaded_table_8_56(1<<i));
    }
    
    return 0;
}
