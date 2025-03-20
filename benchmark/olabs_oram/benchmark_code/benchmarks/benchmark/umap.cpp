#include "./common.h"
using namespace std;

#include "odsl/omap.hpp"
#include "odsl/omap_short_kv.hpp"

const uint64_t KEY_SIZE = 8;
const uint64_t VAL_SIZE = 56;

// using OMap_t = ODSL::OMap<Bytes<KEY_SIZE>, Bytes<VAL_SIZE>, uint32_t>;
using OMap_t = ODSL::OMap<uint64_t, Bytes<VAL_SIZE>, uint32_t>;

using OMapShortKv_t = ODSL::OHashMap<uint64_t, Bytes<VAL_SIZE>, ODSL::ObliviousLevel::FULL_OBLIVIOUS, uint32_t>;


// int benchmark_one_with_initializer(uint64_t N);

int benchmark_umap_shortkv(uint64_t N) {
  // Create and initialize a hashtable
  uint64_t cap = N * 5 / 4; // So it multiplies to get the 80%;
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMapShortKv_t oram = OMapShortKv_t(cap);
  cout << "Initializing ORAM" << endl;
  // oram.Init();
  cout << "ORAM initialized" << endl;
  OMapShortKv_t::InitContext* init = oram.NewInitContext(20ULL * (1ULL<<30));

  uint64_t mod = (N / 20) > 0 ? (N / 20) : 1;
  for (uint64_t i = 0; i < N; i++) {
    if (i % mod == 0)
    {
      BETTER_TEST_LOG("block %zu/%zu", i, N);
    }
    uint64_t key = i;
    Bytes<VAL_SIZE> val;
    val.data[0] = i & 0xff;
    init->Insert(key, val);
    // oram.OInsert(key, val);
  }

  cout << "Finalizing ORAM" << endl;

  init->Finalize();
  delete init;
  BETTER_TEST_LOG("ORAM initialition completed");

  uint64_t end_ns_create = current_time_ns();

  uint64_t start_ns_success = current_time_ns();
  for (uint64_t i=0; i<N; i++) {
    Bytes<VAL_SIZE> val;
    oram.Find(i, val);
  }
  uint64_t end_ns_success = current_time_ns();

  uint64_t start_ns_fail = current_time_ns();
  for (uint64_t i=N; i<cap; i++) {
    Bytes<VAL_SIZE> val;
    oram.Find(i, val);
  }
  uint64_t end_ns_fail = current_time_ns();


  int memAfter = getMemValue();

  double total_ns_success = (double)(end_ns_success - start_ns_success);
  double avg_ns_success   = total_ns_success / (double)N;

  size_t failCount        = cap - N;
  double total_ns_fail    = (double)(end_ns_fail - start_ns_fail);
  double avg_ns_fail      = total_ns_fail / (double)failCount;
  double avg_ns_avg = (avg_ns_success + avg_ns_fail) / 2.0;

  REPORT_LINE("UnorderedMap", "olabs_umap_shortkv", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Initialization_time_us := %.2f", N, KEY_SIZE, VAL_SIZE, (end_ns_create - start_ns_create) / 1000.0);
  REPORT_LINE("UnorderedMap", "olabs_umap_shortkv", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Get_latency_us := %.2f", N, KEY_SIZE, VAL_SIZE, avg_ns_avg / 1000.0);
  REPORT_LINE("UnorderedMap", "olabs_umap_shortkv", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Get_throughput_qps := %.2f", N, KEY_SIZE, VAL_SIZE, 1000000000.0 / avg_ns_avg);
  REPORT_LINE("UnorderedMap", "olabs_umap_shortkv", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Memory_kb := %d", N, KEY_SIZE, VAL_SIZE, memAfter - memBefore);

  return 0;
}


int benchmark_umap(uint64_t N) {
  // Create and initialize a hashtable
  uint64_t cap = N * 5 / 4; // So it multiplies to get the 80%;
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMap_t oram = OMap_t(cap);
  cout << "Initializing ORAM" << endl;
  oram.Init();
  cout << "ORAM initialized" << endl;
  // Intializer_t* init = oram.NewInitContext();

  uint64_t mod = (N / 20) > 0 ? (N / 20) : 1;
  for (uint64_t i = 0; i < N; i++) {
    if (i % mod == 0)
    {
      BETTER_TEST_LOG("block %zu/%zu", i, N);
    }
    uint64_t key = i;
    Bytes<VAL_SIZE> val;
    val.data[0] = i & 0xff;
    oram.OInsert(key, val);
  }

  BETTER_TEST_LOG("ORAM initialition completed");

  // init->Finalize();
  // delete init;

  uint64_t end_ns_create = current_time_ns();

  uint64_t start_ns_success = current_time_ns();
  for (uint64_t i=0; i<N; i++) {
    Bytes<VAL_SIZE> val;
    oram.Find(i, val);
  }
  uint64_t end_ns_success = current_time_ns();

  uint64_t start_ns_fail = current_time_ns();
  for (uint64_t i=N; i<cap; i++) {
    Bytes<VAL_SIZE> val;
    oram.Find(i, val);
  }
  uint64_t end_ns_fail = current_time_ns();


  int memAfter = getMemValue();

  double total_ns_success = (double)(end_ns_success - start_ns_success);
  double avg_ns_success   = total_ns_success / (double)N;

  size_t failCount        = cap - N;
  double total_ns_fail    = (double)(end_ns_fail - start_ns_fail);
  double avg_ns_fail      = total_ns_fail / (double)failCount;
  double avg_ns_avg = (avg_ns_success + avg_ns_fail) / 2.0;

  REPORT_LINE("UnorderedMap", "olabs_umap", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Initialization_time_us := %.2f", N, KEY_SIZE, VAL_SIZE, (end_ns_create - start_ns_create) / 1000.0);
  REPORT_LINE("UnorderedMap", "olabs_umap", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Get_latency_us := %.2f", N, KEY_SIZE, VAL_SIZE, avg_ns_avg / 1000.0);
  REPORT_LINE("UnorderedMap", "olabs_umap", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Get_throughput_qps := %.2f", N, KEY_SIZE, VAL_SIZE, 1000000000.0 / avg_ns_avg);
  REPORT_LINE("UnorderedMap", "olabs_umap", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | fill:=0.8 | Memory_kb := %d", N, KEY_SIZE, VAL_SIZE, memAfter - memBefore);

  return 0;
}


int main() {
  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED(benchmark_umap_shortkv(1<<i));
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED(benchmark_umap(1<<i));
  }

  return 0;
}