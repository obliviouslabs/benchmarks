#include "./common.h"
using namespace std;

#include "odsl/omap.hpp"
#include "odsl/omap_short_kv.hpp"
#include "algorithm/element.hpp"
#include "odsl/adaptive_oram.hpp"
#include "odsl/page_oram.hpp"
#include "odsl/recursive_oram.hpp"

const uint64_t NUM_ACCESSES = 2000000;


template<typename K, typename V>
int benchmark_nroram(uint64_t N) {
  uint64_t memBefore = getMemValue(); 
  uint64_t start_ns_create = current_time_ns();
  ODSL::CircuitORAM::ORAM<V, 2, 20, uint32_t, K, 4096, false>
      oram(N);
  uint64_t end_ns_create = current_time_ns();
  

  uint64_t start_ns_query = current_time_ns(); 
  for (uint64_t i = 0; i < NUM_ACCESSES; i++) {
    V val;
    // uint32_t readPos = rand() % N;
    uint32_t readPos = 0;
    oram.Read(readPos, 0, val);
  }
  uint64_t end_ns_query = current_time_ns();
  int memAfter = getMemValue();

  double avg_ns = (double)(end_ns_query - start_ns_query) / NUM_ACCESSES;
  
  REPORT_LINE("NRORAM", "olabs_oram", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Initialization_zeroed_time_us := %.2f", N, sizeof(K), sizeof(V), (end_ns_create - start_ns_create) / 1000.0);
  REPORT_LINE("NRORAM", "olabs_oram", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Read_latency_us := %.2f", N, sizeof(K), sizeof(V), avg_ns / 1000.0);
  REPORT_LINE("NRORAM", "olabs_oram", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Read_throughput_qps := %.2f", N, sizeof(K), sizeof(V), 1000000000.0 / avg_ns);
  REPORT_LINE("NRORAM", "olabs_oram", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Memory_kb := %d", N, sizeof(K), sizeof(V), memAfter - memBefore);
  return 0;  
}

int benchmark_roram_8bk_8bv(uint64_t N) {
  uint64_t memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();
  ODSL::RecursiveORAM<uint64_t, uint64_t> oram(N);
  uint64_t end_ns_create = current_time_ns();

  uint64_t start_ns_query = current_time_ns();
  for (uint64_t i = 0; i < NUM_ACCESSES; i++) {
    uint64_t val;
    oram.Read(0, val);
  }
  uint64_t end_ns_query = current_time_ns();
  int memAfter = getMemValue();
  double avg_ns = (double)(end_ns_query - start_ns_query) / NUM_ACCESSES;
  REPORT_LINE("RORAM", "olabs_oram", "N:=%zu | Key_bytes := 8 | Value_bytes := 8 | Initialization_zeroed_time_us := %.2f", N, (end_ns_create - start_ns_create) / 1000.0);
  REPORT_LINE("RORAM", "olabs_oram", "N:=%zu | Key_bytes := 8 | Value_bytes := 8 | Read_latency_us := %.2f", N, avg_ns / 1000.0);
  REPORT_LINE("RORAM", "olabs_oram", "N:=%zu | Key_bytes := 8 | Value_bytes := 8 | Read_throughput_qps := %.2f", N, 1000000000.0 / avg_ns);
  REPORT_LINE("RORAM", "olabs_oram", "N:=%zu | Key_bytes := 8 | Value_bytes := 8 | Memory_kb := %d", N, memAfter - memBefore);
  return 0;
}



// Should take less than 20 mins to run
//
int main() {
  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED( (benchmark_nroram<uint32_t,uint32_t>(1<<i)) );
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED( (benchmark_nroram<uint32_t,uint64_t>(1<<i)) );
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED( (benchmark_nroram<uint64_t,uint64_t>(1<<i)) );
  }

  for (uint64_t i = 10; i<=26; i++) {
    RUN_TEST_FORKED(benchmark_roram_8bk_8bv(1<<i));
  }

  return 0;
}