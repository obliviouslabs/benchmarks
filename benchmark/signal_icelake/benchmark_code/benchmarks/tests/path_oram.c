// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <stdio.h>
#include <sys/random.h>

#include "path_oram/path_oram.h"
#include "path_oram/bucket.h"
#include "util/util.h"
#include "util/tests.h"
#include "common.h"

int path_oram_8_8(size_t N)
{
    size_t cap = N; 
    size_t blocks = (N+BLOCK_DATA_SIZE_QWORDS-1)/BLOCK_DATA_SIZE_QWORDS;
    size_t QUERIES = N;
    if (QUERIES > 10000000) {
      QUERIES = 10000000;
    }

    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();

    oram *oram = oram_create(cap, TEST_STASH_SIZE, getentropy);
    oram_allocate_contiguous(oram, blocks-1);
    oram_allocate_block(oram);

    uint64_t end_ns_create = current_time_ns();

    uint64_t start_ns_insertions = current_time_ns();

    u64 buf[BLOCK_DATA_SIZE_QWORDS] = {0};
    for (size_t i = 0; i < QUERIES; ++i)
    {
      uint64_t j = i%BLOCK_DATA_SIZE_QWORDS;
      buf[j] = (i/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j;
      RETURN_IF_ERROR(oram_put(oram, i/BLOCK_DATA_SIZE_QWORDS, buf));
    }

    uint64_t end_ns_insertions = current_time_ns();

    BETTER_TEST_LOG("ORAM initialized and inserted %zu items.\n", QUERIES);

    uint64_t start_ns_queries = current_time_ns();

    for (size_t i = 0; i < QUERIES; ++i)
    {
        u64 buf[BLOCK_DATA_SIZE_QWORDS];
        RETURN_IF_ERROR(oram_get(oram, i/BLOCK_DATA_SIZE_QWORDS, buf));
        uint64_t j = i%BLOCK_DATA_SIZE_QWORDS;
        TEST_ASSERT(buf[j] == (i/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j);
    }

    uint64_t end_ns_queries = current_time_ns();

    int memAfter = getMemValue();

    // Clean up
    oram_destroy(oram);

    // Compute and print average times
    double total_ns_create = (double)(end_ns_create - start_ns_create);

    double total_ns_insertions = (double)(end_ns_insertions - start_ns_insertions);
    double avg_ns_insertions   = total_ns_insertions / (double)QUERIES;

    double total_ns_queries = (double)(end_ns_queries - start_ns_queries);
    double avg_ns_queries   = total_ns_queries / (double)QUERIES;

    REPORT_LINE("RORAM", "Signal", "N:=%zu | Key_bytes := 8 | Value_bytes := 8 | Initialization_zeroed_time_us := %.2f | Read_latency_us := %.2f | Read_throughput_qps := %.2f | Insertion_latency_us := %.2f | Insertion_throughput_qps := %.2f | Memory_kb := %d",
                N, total_ns_create / 1000.0, avg_ns_queries / 1000.0, 1000000000.0 / avg_ns_queries, avg_ns_insertions / 1000.0, 1000000000.0 / avg_ns_insertions, memAfter - memBefore);

    return err_SUCCESS;
}

int path_oram_8_56(size_t N)
{
    uint64_t K = 56/8;
    size_t cap = N; 
    size_t blocks = (N*K+BLOCK_DATA_SIZE_QWORDS-1)/BLOCK_DATA_SIZE_QWORDS;
    size_t QUERIES = N;
    if (QUERIES > 1000000) {
      QUERIES = 1000000;
    }

    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();

    oram *oram = oram_create(K*cap, TEST_STASH_SIZE, getentropy);
    oram_allocate_contiguous(oram, blocks-1);
    oram_allocate_block(oram);

    uint64_t end_ns_create = current_time_ns();

    uint64_t start_ns_insertions = current_time_ns();

    u64 buf[BLOCK_DATA_SIZE_QWORDS] = {0};
    for (size_t i = 0; i < QUERIES; ++i)
    {
      uint64_t j = (i*K)%BLOCK_DATA_SIZE_QWORDS;
      for (size_t l = 0; l < K; ++l)
      {
          buf[j+l] = (i*K/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j;
      }
      RETURN_IF_ERROR(oram_put(oram, i*K/BLOCK_DATA_SIZE_QWORDS, buf));
    }

    uint64_t end_ns_insertions = current_time_ns();

    BETTER_TEST_LOG("ORAM initialized and inserted %zu items.\n", QUERIES);

    uint64_t start_ns_queries = current_time_ns();

    for (size_t i = 0; i < QUERIES; ++i)
    {
        u64 buf[BLOCK_DATA_SIZE_QWORDS];
        uint64_t j = (i*K)%BLOCK_DATA_SIZE_QWORDS;
        RETURN_IF_ERROR(oram_get(oram, i*K/BLOCK_DATA_SIZE_QWORDS, buf));
        for (size_t l = 0; l < K; ++l)
       {
            TEST_ASSERT(buf[j+l] == (i*K/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j);
        }
    }

    uint64_t end_ns_queries = current_time_ns();

    int memAfter = getMemValue();

    // Clean up
    oram_destroy(oram);

    // Compute and print average times
    double total_ns_create = (double)(end_ns_create - start_ns_create);

    double total_ns_insertions = (double)(end_ns_insertions - start_ns_insertions);
    double avg_ns_insertions   = total_ns_insertions / (double)QUERIES;

    double total_ns_queries = (double)(end_ns_queries - start_ns_queries);
    double avg_ns_queries   = total_ns_queries / (double)QUERIES;

    REPORT_LINE("RORAM", "Signal", "N:=%zu | Key_bytes := 8 | Value_bytes := 56 | Initialization_zeroed_time_us := %.2f | Read_latency_us := %.2f | Read_throughput_qps := %.2f | Insertion_latency_us := %.2f | Insertion_throughput_qps := %.2f | Memory_kb := %d",
                N, total_ns_create / 1000.0, avg_ns_queries / 1000.0, 1000000000.0 / avg_ns_queries, avg_ns_insertions / 1000.0, 1000000000.0 / avg_ns_insertions, memAfter - memBefore);

    return err_SUCCESS;
}

int main()
{
    // Should take __ to run
    for (uint64_t i = 10; i <= 28; i++) {
        RUN_TEST_FORKED(path_oram_8_8(1<<i));
    }

    for (uint64_t i = 10; i <= 28; i++) {
        RUN_TEST_FORKED(path_oram_8_56(1<<i));
    }
    
    return 0;
}


