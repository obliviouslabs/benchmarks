#include "./common.h"
using namespace std;

#include "odsl/par_omap.hpp"
#include "odsl/omap_short_kv.hpp"

const uint64_t KEY_SIZE = 8; // We just use uint64_t directly bellow
const uint64_t VAL_SIZE = 56;
const uint64_t NUM_SHARDS = 15;

// using OMap_t = ODSL::OMap<Bytes<KEY_SIZE>, Bytes<VAL_SIZE>, uint32_t>;
using OMap_t = ODSL::ParOMap<uint64_t, Bytes<VAL_SIZE>, uint32_t>;


// int benchmark_one_with_initializer(uint64_t N);

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
  size_t queries_batch_size = batch_size;
  
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMap_t oram = OMap_t(cap, NUM_SHARDS);
  cout << "Initializing ORAM" << endl;
  // oram.Init();
  cout << "ORAM initialized" << endl;
  cout << "omp max threads: " << omp_get_max_threads() << endl;

  OMap_t::InitContext* init = oram.NewInitContext(N, 20ULL * (1ULL<<30));

  uint64_t mod = (N / 20) > 0 ? (N / 20) : 1;
  for (uint64_t i = 0; i < N; i++) {
    if (i % mod == 0)
    {
      BETTER_TEST_LOG("block %zu/%zu", i, N);
    }
    uint64_t key = i;
    Bytes<VAL_SIZE> val;
    val.data[0] = i & 0xff;
    // We don't care about receiving things in batches, we just insert them to the initializer directly
    init->Insert(key, val);
  }

  cout << "Finalizing ORAM" << endl;

  init->Finalize();
  delete init;
  BETTER_TEST_LOG("ORAM initialition completed");

  uint64_t end_ns_create = current_time_ns();

  uint64_t start_query_time = current_time_ns();
  std::vector<uint64_t> queries(queries_batch_size);
  std::vector<Bytes<VAL_SIZE>> results(queries_batch_size);
  uint64_t curr = N - 1;
  for (uint64_t i=0; i<num_queries; i+=queries_batch_size) {    
    for (uint64_t j=0; j<queries_batch_size; j++) {
      queries[j] = curr;
      curr--;
    }
    std::vector<uint8_t> findExistFlag =
      oram.FindBatch(queries.begin(), queries.end(), results.begin());
  }
  uint64_t end_query_time = current_time_ns();


  int memAfter = getMemValue();

  double avg_ns_success = (double)(end_query_time - start_query_time) / num_queries;

  REPORT_LINE("UnorderedMap", "olabs_umap_sharded", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Shards:= %d | Batch_size := %zu | fill:=0.8 | Initialization_time_us := %.2f", N, KEY_SIZE, VAL_SIZE, NUM_SHARDS, batch_size, (end_ns_create - start_ns_create) / 1000.0);
  REPORT_LINE("UnorderedMap", "olabs_umap_sharded", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Shards:= %d | Batch_size := %zu | fill:=0.8 | Get_latency_us := %.2f", N, KEY_SIZE, VAL_SIZE, NUM_SHARDS, batch_size, avg_ns_success / 1000.0 * batch_size);
  REPORT_LINE("UnorderedMap", "olabs_umap_sharded", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Shards:= %d | Batch_size := %zu | fill:=0.8 | Get_throughput_qps := %.2f", N, KEY_SIZE, VAL_SIZE, NUM_SHARDS, batch_size, 1000000000.0 / avg_ns_success);
  REPORT_LINE("UnorderedMap", "olabs_umap_sharded", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Shards:= %d | Batch_size := %zu | fill:=0.8 | Memory_kb := %d", N, KEY_SIZE, VAL_SIZE, NUM_SHARDS, batch_size, memAfter - memBefore);

  return 0;
}


int main() {
  for (uint64_t j = 10; j<=20; j++) {
    RUN_TEST_FORKED(benchmark_umap_sharded(1<<j, NUM_SHARDS));
}

for (uint64_t j = 10; j<=22; j++) {
    RUN_TEST_FORKED(benchmark_umap_sharded(1<<j, 100));
}

uint64_t batch_sizes[] = {1000,4096,8192};
for (uint64_t i = 0; i<3; i++) {
    for (uint64_t j = 10; j<=26; j++) {
        RUN_TEST_FORKED(benchmark_umap_sharded(1<<j, batch_sizes[i]));
    }
}
  
  return 0;
}