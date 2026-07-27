// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/random.h>

#include <time.h>

#include "sharded_ohtable/sharded_ohtable.h"
#include "util/util.h"
#include "util/tests.h"
#include "ohtable/ohtable.h"
#include "common.h"

#ifndef SIGNAL_JASMINE_PATH_LENGTH
#define SIGNAL_JASMINE_PATH_LENGTH 0
#endif

typedef struct
{
    uint64_t e164;
    uint64_t aci[2];
    uint64_t pni[2];
    uint64_t uak[2];
} record;

#define NUM_SHARDS 16

#define RECORDS_TO_INSERT (1<<20)
#define RECORD_SIZE_QWORDS 7
u64 insert_records[RECORD_SIZE_QWORDS * RECORDS_TO_INSERT];
u64 queries[RECORDS_TO_INSERT];
u64 notfound_queries[RECORDS_TO_INSERT];

u8 hash_key[8] = {233, 144, 89, 55, 34, 21, 13, 8};


typedef struct run_shard_args
{
    sharded_ohtable *table;
    size_t shard_id;
} run_shard_args;

void *shard_thread_func(void *input)
{
    run_shard_args *args = input;
    sharded_ohtable_run_shard(args->table, args->shard_id);
    free(args);
    return 0;
}

pthread_t start_shard_thread(sharded_ohtable *table, size_t shard_id)
{
    pthread_t tid;
    run_shard_args *args = calloc(1, sizeof(*args));
    args->table = table;
    args->shard_id = shard_id;

    pthread_create(&tid, NULL, shard_thread_func, args);

    return tid;
}

size_t sharded_table_capacity_records(sharded_ohtable *table)
{
    size_t capacity = 0;
    ohtable_statistics **stats = sharded_ohtable_report_statistics(table);
    CHECK(stats != NULL);
    for (size_t i = 0; i < NUM_SHARDS; ++i)
    {
        capacity += stats[i]->capacity;
        ohtable_statistics_destroy(stats[i]);
    }
    free(stats);
    return capacity;
}

void prepare_queries_and_inserts()
{
    memset(insert_records, 0, sizeof(insert_records));
    memset(queries, 0, sizeof(queries));
    memset(notfound_queries, 0, sizeof(notfound_queries));
    for (size_t i = 0; i < RECORDS_TO_INSERT; ++i)
    {
        uint64_t buf[1];
        getentropy(buf, sizeof(buf));
        u64 id = i == 0 ? 3061075565009377566ull : i; // 3061075565009377566ull hashes to same location as 0 and can cause fun
        insert_records[RECORD_SIZE_QWORDS * i] = id;
        queries[i] = id;
        notfound_queries[i] = id + RECORDS_TO_INSERT;
    }
}

int test_loaded_sharded_table(size_t N, const size_t batch_size)
{

  size_t num_records_to_add = N;
  size_t num_queries = N;
  size_t queries_batch_size = batch_size;

  prepare_queries_and_inserts();
  size_t records_per_batch_insertion = batch_size;
  size_t num_blocks = (num_records_to_add+records_per_batch_insertion-1) / records_per_batch_insertion;

  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  sharded_ohtable *table = sharded_ohtable_create(RECORD_SIZE_QWORDS, NUM_SHARDS, hash_key, getentropy);
  size_t table_capacity = sharded_table_capacity_records(table);
  if (num_records_to_add > table_capacity)
  {
      LOG_INFO("Skipping N=%zu; Jasmine table capacity is %zu records\n", num_records_to_add, table_capacity);
      sharded_ohtable_destroy(table);
      return err_SUCCESS;
  }
  double fill = (double)num_records_to_add / (double)table_capacity;

  pthread_t shard_tids[NUM_SHARDS];
  for (size_t i = 0; i < NUM_SHARDS; ++i)
  {
      shard_tids[i] = start_shard_thread(table, i);
  }

  uint64_t intermediate_ns_create = current_time_ns();

  record *data = calloc(records_per_batch_insertion, sizeof(*data));
  TEST_LOG("adding %zu blocks for %zu total records\n", num_blocks, num_records_to_add);

  uint64_t curr = 0;
  uint64_t mod = (num_blocks / 20) > 0 ? (num_blocks / 20) : 1;
  for (size_t block = 0; block < num_blocks; ++block)
  {
      if (block % mod == 0)
      {
          fprintf(stderr, "block %zu/%zu\n", block, num_blocks);
      }
      size_t records_this_batch = num_records_to_add - block * records_per_batch_insertion;
      if (records_this_batch > records_per_batch_insertion)
      {
          records_this_batch = records_per_batch_insertion;
      }
      memset(data, 0, records_per_batch_insertion * sizeof(*data));
      for (size_t i = 0; i < records_this_batch; ++i)
      {
          uint64_t buf[1];
          // getentropy(buf, sizeof(buf));
          buf[0] = curr++;
          data[i].e164 = buf[0];
      }
      RETURN_IF_ERROR(sharded_ohtable_put_batch(table, records_this_batch, (u64 *)data));
  }
  free(data);

  uint64_t end_ns_create = current_time_ns();

  uint64_t start_query_time = current_time_ns();


  u64 *results = calloc(queries_batch_size * RECORD_SIZE_QWORDS, sizeof(*results));
  CHECK(results != NULL);
  for (uint64_t i=0; i<num_queries; i+=queries_batch_size) {
    size_t this_batch_size = (num_queries - i) < queries_batch_size ? (num_queries - i) : queries_batch_size;
    for (uint64_t j=0; j<this_batch_size; j++) {
      queries[j] = curr;
      curr--;
    }
    RETURN_IF_ERROR(sharded_ohtable_get_batch(table, this_batch_size, queries, results));
  }
  free(results);


  uint64_t end_query_time = current_time_ns();

  int memAfter = getMemValue();  

  double avg_ns_success = (double)(end_query_time - start_query_time) / num_queries;

  REPORT_LINE("UnorderedMap", "Signal_Jasmine_Sharded", "N:=%zu | Path_length := %d | Batch_size:=%zu | Key_bytes := 8 | Value_bytes := 56 | Shards:= %d | Capacity_records := %zu | fill:=%.4f | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Memory_kb := %d", N, SIGNAL_JASMINE_PATH_LENGTH, batch_size, NUM_SHARDS, table_capacity, fill, (end_ns_create - start_ns_create) / 1000.0, queries_batch_size * avg_ns_success / 1000.0, 1000000000.0 / avg_ns_success, memAfter - memBefore);
  LOG_INFO("Of which creating the thread: %.2f ms\n", (intermediate_ns_create - start_ns_create) / 1e6);


  // Cleanup:
  //
  for (size_t i = 0; i < NUM_SHARDS; ++i)
  {
      sharded_ohtable_stop_shard(table, i);
      pthread_join(shard_tids[i], 0);
  }

    sharded_ohtable_destroy(table);

    return err_SUCCESS;
}

int main(int argc, char **argv)
{
    // We run the tests up to 40GB of memory usage, and avoid slower tests for non optimal batch sizes
    //
    // Should take less than 2h to run
    uint64_t batch_sizes[] = {NUM_SHARDS,1024,4096,8192,65536,1048576};
    uint64_t BEST_IDX = 4; // 65536 is the best batch size for signal sharded table
    bool run_best_only = false;

    if (argc > 1 && strcmp(argv[1], "best") == 0) {
        run_best_only = true;
    }
    for (uint64_t i = 0; i<6; i++) {
        if (run_best_only && i != BEST_IDX) {
            continue;
        }
        for (uint64_t j = 10; j <= 28; j++) {
            if (i == 0 && j >= 24) {
                continue;
            }
            RUN_TEST_FORKED(test_loaded_sharded_table(1<<j, batch_sizes[i]));
        }
    }

    return 0;
}
