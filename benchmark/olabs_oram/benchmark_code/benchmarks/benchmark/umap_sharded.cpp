#include "./common.h"
using namespace std;

#include "odsl/par_omap.hpp"
#include "odsl/omap_short_kv.hpp"

const uint64_t NUM_SHARDS = 15;

template <size_t KEY_SIZE>
struct BytesHelper
{
  using type = Bytes<KEY_SIZE>;
  
  static type key_from_u64(uint64_t x) {
    type key = type::DUMMY();
    uint64_t bytes_to_copy = KEY_SIZE > 8 ? 8 : KEY_SIZE;
    for (uint64_t i = 0; i < bytes_to_copy; ++i) {
      key.data[i] = x & 0xff;
      x >>= 8;
    }
    return key;
  }
};

template <>
struct BytesHelper<8> {
  using type = uint64_t;

  static type key_from_u64(uint64_t origin) {
    return origin;
  }
};

template <>
struct BytesHelper<4> {
  using type = uint32_t;

  static type key_from_u64(uint64_t origin) {
    return origin;
  }
};

template<size_t KEY_SIZE, size_t VAL_SIZE>
using OMap_t = ODSL::ParOMap<typename BytesHelper<KEY_SIZE>::type, typename BytesHelper<VAL_SIZE>::type, uint32_t>;

// int benchmark_one_with_initializer(uint64_t N);

template<size_t KEY_SIZE, size_t VAL_SIZE>
int benchmark_umap_sharded(uint64_t N, size_t batch_size) {
  if (EM::Backend::g_DefaultBackend) {
    delete EM::Backend::g_DefaultBackend;
  }
  size_t BackendSize = 20ULL * (1ULL<<30); // Give it a lot of RAM
  EM::Backend::g_DefaultBackend =
      new EM::Backend::MemServerBackend(BackendSize);
  // Create and initialize a hashtable
  uint64_t cap = N * 5 / 4; // So it multiplies to get the 80%;
  size_t num_queries = N;
  while (num_queries*batch_size > 250000000) { // Keep test time reasonable for large batches
    num_queries /= 2;
  }
  size_t queries_batch_size = batch_size;
  
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMap_t<KEY_SIZE, VAL_SIZE> oram = OMap_t<KEY_SIZE, VAL_SIZE>(cap, NUM_SHARDS);
  cout << "Initializing ORAM" << endl;
  // oram.Init();
  cout << "ORAM initialized" << endl;
  cout << "omp max threads: " << omp_get_max_threads() << endl;

  typename OMap_t<KEY_SIZE, VAL_SIZE>::InitContext* init = oram.NewInitContext(N, 20ULL * (1ULL<<30));

  uint64_t mod = (N / 20) > 0 ? (N / 20) : 1;
  for (uint64_t i = 0; i < N; i++) {
    if (i % mod == 0)
    {
      BETTER_TEST_LOG("block %zu/%zu", i, N);
    }
    typename BytesHelper<KEY_SIZE>::type key = BytesHelper<KEY_SIZE>::key_from_u64(i);
    typename BytesHelper<VAL_SIZE>::type val = BytesHelper<VAL_SIZE>::key_from_u64(i & 0xff);
    // We don't care about receiving things in batches, we just insert them to the initializer directly
    init->Insert(key, val);
  }

  cout << "Finalizing ORAM" << endl;

  init->Finalize();
  delete init;
  BETTER_TEST_LOG("ORAM initialition completed");

  uint64_t end_ns_create = current_time_ns();

  uint64_t start_query_time = current_time_ns();
  std::vector<typename BytesHelper<KEY_SIZE>::type> queries(queries_batch_size);
  std::vector<typename BytesHelper<VAL_SIZE>::type> results(queries_batch_size);
  uint64_t curr = N - 1;
  for (uint64_t i=0; i<num_queries; i+=queries_batch_size) {    
    for (uint64_t j=0; j<queries_batch_size; j++) {
      queries[j] = BytesHelper<KEY_SIZE>::key_from_u64(curr);
      curr--;
    }
    std::vector<uint8_t> findExistFlag =
      oram.FindBatch(queries.begin(), queries.end(), results.begin());
  }
  uint64_t end_query_time = current_time_ns();

  int memAfter = getMemValue();

  double avg_ns_success = (double)(end_query_time - start_query_time) / num_queries;

  REPORT_LINE("UnorderedMap", "olabs_oram_sharded", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Shards := %d | Batch_size := %zu | fill:=0.8 | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Memory_kb := %d", N, KEY_SIZE, VAL_SIZE, NUM_SHARDS, batch_size, (end_ns_create - start_ns_create) / 1000.0, avg_ns_success / 1000.0 * batch_size, 1000000000.0 / avg_ns_success, memAfter - memBefore);

  return 0;
}


// UNDONE(): Should take less than 4h to run
int main() {
  uint64_t batch_sizes[] = {NUM_SHARDS,1024,4096,8192,65536,1<<20};

  for (uint64_t i = 0; i<6; i++) {
    for (uint64_t j = 10; j<=28; j++) {
      if (i == 0 && j >= 24) {
        continue;
      }
      RUN_TEST_FORKED((benchmark_umap_sharded<8,8>(1<<j, batch_sizes[i])));
      RUN_TEST_FORKED((benchmark_umap_sharded<8,56>(1<<j, batch_sizes[i])));
      RUN_TEST_FORKED((benchmark_umap_sharded<32,32>(1<<j, batch_sizes[i])));
    }
  }

  // UNDONE(): Add 8bk8bv?
  return 0;
}