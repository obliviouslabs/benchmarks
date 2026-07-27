// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "path_oram/path_oram.h"
#include "path_oram/bucket.h"
#include "util/util.h"
#include "util/tests.h"
#include "common.h"

#ifndef SIGNAL_JASMINE_PATH_LENGTH
#define SIGNAL_JASMINE_PATH_LENGTH 0
#endif

static size_t min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

static size_t benchmark_operation_limit(size_t default_limit)
{
    const char *value = getenv("SIGNAL_JASMINE_ORAM_MAX_OPS");
    if (value == NULL || value[0] == '\0')
    {
        return default_limit;
    }

    char *end = NULL;
    uint64_t parsed = strtoull(value, &end, 10);
    if (end == value || *end != '\0')
    {
        return default_limit;
    }
    if (parsed == 0)
    {
        return (size_t)-1;
    }
    if (parsed > (uint64_t)((size_t)-1))
    {
        return (size_t)-1;
    }
    return (size_t)parsed;
}

static size_t records_to_blocks(size_t records, size_t qwords_per_record)
{
    size_t qwords = records * qwords_per_record;
    return (qwords + BLOCK_DATA_SIZE_QWORDS - 1) / BLOCK_DATA_SIZE_QWORDS;
}

static size_t oram_capacity_records(oram *oram, size_t qwords_per_record)
{
    return (oram_capacity_blocks(oram) * BLOCK_DATA_SIZE_QWORDS) / qwords_per_record;
}

int path_oram_8_8(size_t N)
{
    const size_t qwords_per_record = 1;

    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();

    oram *oram = oram_create_depth16(getentropy);
    size_t capacity_blocks = oram_capacity_blocks(oram);
    size_t capacity_records = oram_capacity_records(oram, qwords_per_record);
    if (N == 0)
    {
        N = capacity_records;
    }
    size_t blocks = records_to_blocks(N, qwords_per_record);
    if (blocks > capacity_blocks)
    {
      LOG_INFO("Skipping N=%zu; Jasmine ORAM capacity is %zu records across %zu blocks\n", N, capacity_records, capacity_blocks);
      oram_destroy(oram);
      return err_SUCCESS;
    }
    size_t operations = min_size(N, benchmark_operation_limit(10000000));
    size_t inserted_blocks = records_to_blocks(operations, qwords_per_record);

    uint64_t end_ns_create = current_time_ns();

    uint64_t start_ns_insertions = current_time_ns();

    u64 buf[BLOCK_DATA_SIZE_QWORDS] = {0};
    for (size_t i = 0; i < operations; ++i)
    {
      uint64_t j = i%BLOCK_DATA_SIZE_QWORDS;
      buf[j] = (i/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j;
      RETURN_IF_ERROR(oram_put(oram, i/BLOCK_DATA_SIZE_QWORDS, buf));
    }

    uint64_t end_ns_insertions = current_time_ns();

    BETTER_TEST_LOG("ORAM initialized and inserted %zu items.\n", operations);

    uint64_t start_ns_queries = current_time_ns();

    for (size_t i = 0; i < operations; ++i)
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
    double avg_ns_insertions   = total_ns_insertions / (double)operations;

    double total_ns_queries = (double)(end_ns_queries - start_ns_queries);
    double avg_ns_queries   = total_ns_queries / (double)operations;

    REPORT_LINE("RORAM", "Signal_Jasmine", "N:=%zu | Path_length := %d | Key_bytes := 8 | Value_bytes := 8 | Capacity_blocks := %zu | Capacity_records := %zu | Inserted_records := %zu | Query_count := %zu | Target_fill:=%.4f | fill:=%.4f | Initialization_zeroed_time_us := %.2f | Read_latency_us := %.2f | Read_throughput_qps := %.2f | Insertion_latency_us := %.2f | Insertion_throughput_qps := %.2f | Memory_kb := %d",
                N, SIGNAL_JASMINE_PATH_LENGTH, capacity_blocks, capacity_records, operations, operations, (double)N / (double)capacity_records, (double)inserted_blocks / (double)capacity_blocks, total_ns_create / 1000.0, avg_ns_queries / 1000.0, 1000000000.0 / avg_ns_queries, avg_ns_insertions / 1000.0, 1000000000.0 / avg_ns_insertions, memAfter - memBefore);

    return err_SUCCESS;
}

int path_oram_8_56(size_t N)
{
    const size_t qwords_per_record = 56 / 8;

    int memBefore = getMemValue();
    uint64_t start_ns_create = current_time_ns();

    oram *oram = oram_create_depth16(getentropy);
    size_t capacity_blocks = oram_capacity_blocks(oram);
    size_t capacity_records = oram_capacity_records(oram, qwords_per_record);
    if (N == 0)
    {
        N = capacity_records;
    }
    size_t blocks = records_to_blocks(N, qwords_per_record);
    if (blocks > capacity_blocks)
    {
      LOG_INFO("Skipping N=%zu; Jasmine ORAM capacity is %zu records across %zu blocks\n", N, capacity_records, capacity_blocks);
      oram_destroy(oram);
      return err_SUCCESS;
    }
    size_t operations = min_size(N, benchmark_operation_limit(1000000));
    size_t inserted_blocks = records_to_blocks(operations, qwords_per_record);

    uint64_t end_ns_create = current_time_ns();

    uint64_t start_ns_insertions = current_time_ns();

    u64 buf[BLOCK_DATA_SIZE_QWORDS] = {0};
    for (size_t i = 0; i < operations; ++i)
    {
      uint64_t j = (i*qwords_per_record)%BLOCK_DATA_SIZE_QWORDS;
      for (size_t l = 0; l < qwords_per_record; ++l)
      {
          buf[j+l] = (i*qwords_per_record/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j;
      }
      RETURN_IF_ERROR(oram_put(oram, i*qwords_per_record/BLOCK_DATA_SIZE_QWORDS, buf));
    }

    uint64_t end_ns_insertions = current_time_ns();

    BETTER_TEST_LOG("ORAM initialized and inserted %zu items.\n", operations);

    uint64_t start_ns_queries = current_time_ns();

    for (size_t i = 0; i < operations; ++i)
    {
        u64 buf[BLOCK_DATA_SIZE_QWORDS];
        uint64_t j = (i*qwords_per_record)%BLOCK_DATA_SIZE_QWORDS;
        RETURN_IF_ERROR(oram_get(oram, i*qwords_per_record/BLOCK_DATA_SIZE_QWORDS, buf));
        for (size_t l = 0; l < qwords_per_record; ++l)
       {
            TEST_ASSERT(buf[j+l] == (i*qwords_per_record/BLOCK_DATA_SIZE_QWORDS)*BLOCK_DATA_SIZE_QWORDS*BLOCK_DATA_SIZE_QWORDS + j);
        }
    }

    uint64_t end_ns_queries = current_time_ns();

    int memAfter = getMemValue();

    // Clean up
    oram_destroy(oram);

    // Compute and print average times
    double total_ns_create = (double)(end_ns_create - start_ns_create);

    double total_ns_insertions = (double)(end_ns_insertions - start_ns_insertions);
    double avg_ns_insertions   = total_ns_insertions / (double)operations;

    double total_ns_queries = (double)(end_ns_queries - start_ns_queries);
    double avg_ns_queries   = total_ns_queries / (double)operations;

    REPORT_LINE("RORAM", "Signal_Jasmine", "N:=%zu | Path_length := %d | Key_bytes := 8 | Value_bytes := 56 | Capacity_blocks := %zu | Capacity_records := %zu | Inserted_records := %zu | Query_count := %zu | Target_fill:=%.4f | fill:=%.4f | Initialization_zeroed_time_us := %.2f | Read_latency_us := %.2f | Read_throughput_qps := %.2f | Insertion_latency_us := %.2f | Insertion_throughput_qps := %.2f | Memory_kb := %d",
                N, SIGNAL_JASMINE_PATH_LENGTH, capacity_blocks, capacity_records, operations, operations, (double)N / (double)capacity_records, (double)inserted_blocks / (double)capacity_blocks, total_ns_create / 1000.0, avg_ns_queries / 1000.0, 1000000000.0 / avg_ns_queries, avg_ns_insertions / 1000.0, 1000000000.0 / avg_ns_insertions, memAfter - memBefore);

    return err_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "capacity") == 0)
    {
        RUN_TEST_FORKED(path_oram_8_8(0));
        RUN_TEST_FORKED(path_oram_8_56(0));
        return 0;
    }

    for (uint64_t i = 10; i <= 28; i++) {
        RUN_TEST_FORKED(path_oram_8_8((size_t)1<<i));
    }

    for (uint64_t i = 10; i <= 28; i++) {
        RUN_TEST_FORKED(path_oram_8_56((size_t)1<<i));
    }

    return 0;
}
