#include "./common.h"
using namespace std;

#include "odsl/omap.hpp"
#include "odsl/omap_short_kv.hpp"


template<size_t KEY_SIZE, size_t VAL_SIZE>
using OMapShortKv_t = ODSL::OHashMap<Bytes<KEY_SIZE>, Bytes<VAL_SIZE>, ODSL::ObliviousLevel::FULL_OBLIVIOUS, uint32_t>;

template<size_t KEY_SIZE, size_t VAL_SIZE>
using OMap_t = ODSL::OMap<Bytes<KEY_SIZE>, Bytes<VAL_SIZE>, uint32_t>;


template<size_t KEY_SIZE>
Bytes<KEY_SIZE> key_from_u64(uint64_t origin) {
  Bytes<KEY_SIZE> key = Bytes<KEY_SIZE>::DUMMY();
  uint64_t bytes_to_copy = KEY_SIZE;
  if (bytes_to_copy > 8) {
    bytes_to_copy = 8;
  }
  for (uint64_t i = 0; i < bytes_to_copy; ++i) {
    key.data[i] = origin & 0xff; // Fill with dummy data
    origin >>= 1;
  }
  return key;
}

// int benchmark_one_with_initializer(uint64_t N);

template<size_t KEY_SIZE, size_t VAL_SIZE>
int benchmark_umap_shortkv(uint64_t N) {

  // Create and initialize a hashtable
  uint64_t cap = N * 5 / 4; // So it multiplies to get the 80%;
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMapShortKv_t<KEY_SIZE,VAL_SIZE> oram = OMapShortKv_t<KEY_SIZE,VAL_SIZE>(cap);
  cout << "Initializing ORAM" << endl;
  // oram.Init();
  cout << "ORAM initialized" << endl;
  typename OMapShortKv_t<KEY_SIZE,VAL_SIZE>::InitContext* init = oram.NewInitContext(20ULL * (1ULL<<30));

  uint64_t mod = (N / 20) > 0 ? (N / 20) : 1;
  for (uint64_t i = 0; i < N; i++) {
    if (i % mod == 0)
    {
      BETTER_TEST_LOG("block %zu/%zu", i, N);
    }
    Bytes<KEY_SIZE> key = key_from_u64<KEY_SIZE>(i);
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
    Bytes<KEY_SIZE> key = key_from_u64<KEY_SIZE>(i);
    oram.Find(key, val);
  }
  uint64_t end_ns_success = current_time_ns();

  uint64_t start_ns_fail = current_time_ns();
  for (uint64_t i=N; i<cap; i++) {
    Bytes<VAL_SIZE> val;
    Bytes<KEY_SIZE> key = key_from_u64<KEY_SIZE>(i);
    oram.Find(key, val);
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

template<size_t KEY_SIZE, size_t VAL_SIZE>
int benchmark_umap(uint64_t N) {
  // Create and initialize a hashtable
  uint64_t cap = N * 5 / 4; // So it multiplies to get the 80%;
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMap_t<KEY_SIZE,VAL_SIZE> oram = OMap_t<KEY_SIZE,VAL_SIZE>(cap);
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
    Bytes<KEY_SIZE> key = key_from_u64<KEY_SIZE>(i);
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
    Bytes<KEY_SIZE> key = key_from_u64<KEY_SIZE>(i);
    Bytes<VAL_SIZE> val;
    oram.Find(key, val);
  }
  uint64_t end_ns_success = current_time_ns();

  uint64_t start_ns_fail = current_time_ns();
  for (uint64_t i=N; i<cap; i++) {
    Bytes<KEY_SIZE> key = key_from_u64<KEY_SIZE>(i);
    Bytes<VAL_SIZE> val;
    oram.Find(key, val);
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

// UNDONE(): Should take less then 1h to run
int main() {
  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED((benchmark_umap_shortkv<8,8>(1<<i)));
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED((benchmark_umap_shortkv<32,32>(1<<i)));
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED((benchmark_umap_shortkv<8,56>(1<<i)));
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED((benchmark_umap<8,8>(1<<i)));
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED((benchmark_umap<32,32>(1<<i)));
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED((benchmark_umap<8,56>(1<<i)));
  }

  return 0;
}