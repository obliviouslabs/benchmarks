// This file is for temporary benchmarks only, please ignore.
// 
#include "./common.h"
#include "cds.pb.h"
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/delimited_message_util.h>

using namespace std;

#include "odsl/par_omap.hpp"
#include "odsl/omap_short_kv.hpp"
#include <format>

const uint64_t NUM_SHARDS = 15;

struct Tags {
  std::array<uint8_t, 16> aci;
  std::array<uint8_t, 16> pni;
  std::array<uint8_t, 16> uak;
};

using OMap_t = ODSL::ParOMap<uint64_t, Tags, uint32_t>;


int benchmark_umap_sharded(uint64_t N, uint64_t Q, size_t batch_size) {
  if (EM::Backend::g_DefaultBackend) {
    delete EM::Backend::g_DefaultBackend;
  }
  size_t BackendSize = 20ULL * (1ULL<<30); // Give it a lot of RAM
  EM::Backend::g_DefaultBackend =
      new EM::Backend::MemServerBackend(BackendSize);

  // load_requests_1000000.dat
  string load_filename = format("../../../signalbench/data/load_requests_{}.dat", N);
  string query_filename = format("../../../signalbench/data/query_requests_{}_{}.dat", N, Q);

  // Create and initialize a hashtable
  uint64_t cap = N * 5 / 4; // So it multiplies to get the 80%;
  
  int memBefore = getMemValue();
  uint64_t start_ns_create = current_time_ns();

  cout << "N: " << N << endl;
  cout << "Creating ORAM with capacity " << cap << endl;
  OMap_t oram = OMap_t(cap, NUM_SHARDS);
  cout << "Initializing ORAM" << endl;
  // oram.Init();
  cout << "ORAM initialized" << endl;
  cout << "omp max threads: " << omp_get_max_threads() << endl;

  typename OMap_t::InitContext* init = oram.NewInitContext(N, 20ULL * (1ULL<<30));

  std::ifstream ifs(load_filename, std::ios::binary);
  if (!ifs) {
    cerr << "Error opening file: " << load_filename << endl;
    return 1;
  }

  google::protobuf::io::IstreamInputStream zc(&ifs);
  bool clean_eof = false;
  size_t batch_idx = 0;
  size_t total_records = 0;

  uint64_t mod = (N / 20) > 0 ? (N / 20) : 1;
  while (true) {
    bench::cds::CDSLoadBatch batch;
    if (!google::protobuf::util::ParseDelimitedFromZeroCopyStream(&batch, &zc, &clean_eof)) {
      if (clean_eof) break;
      std::cerr << "Parse error reading delimited CDSLoadBatch\n";
      return 1;
    }
    if (clean_eof) break;

    const std::string& bytes = batch.records();
    const uint8_t* p = reinterpret_cast<const uint8_t*>(bytes.data());
    const size_t n   = bytes.size() / 56;

    for (size_t i = 0; i < n; ++i) {
      const uint8_t* base = p + i * 56;
      uint64_t  key = *(uint64_t*) base;
      Tags tags;
      std::copy(base+8, base+24, tags.aci.begin());
      std::copy(base+24, base+40, tags.pni.begin());
      std::copy(base+40, base+56, tags.uak.begin());

      // We don't care about receiving things in batches, we just want to insert them in the initializer directly
      init->Insert(key, tags);
    }
  }

  cout << "Finalizing ORAM" << endl;

  init->Finalize();
  delete init;
  BETTER_TEST_LOG("ORAM initialition completed");

  uint64_t end_ns_create = current_time_ns();

  std::ifstream qfs(query_filename, std::ios::binary);
  if (!qfs) {
    std::cerr << "Error opening file: " << query_filename << std::endl;
    return 1;
  }
  google::protobuf::io::IstreamInputStream zc_q(&qfs);
  

  uint64_t start_query_time = current_time_ns();
  std::vector<uint64_t> queries(batch_size);
  std::vector<Tags> results(batch_size);
  uint64_t curr = N - 1;
  uint64_t total_records_queried = 0;
  uint64_t total_db_batches = 0;
  uint64_t fill = 0;
  clean_eof = false;


  while(true) {
    bench::cds::CDSQueryBatch qbatch;
    if (!google::protobuf::util::ParseDelimitedFromZeroCopyStream(&qbatch, &zc_q, &clean_eof)) {
      if (clean_eof) break;
      std::cerr << "Parse error reading CDSQueryBatch\n";
      return 1;
    }
    if (clean_eof) break;

    const auto& e164s = qbatch.e164s();
    const size_t M = static_cast<size_t>(e164s.size());
    if (M == 0) continue;          // skip empty

    size_t i = 0;
    while (i < M) {
      const size_t space = batch_size - fill;
      const size_t take  = std::min(space, M - i);

      // append keys into current batch buffer
      for (size_t j = 0; j < take; ++j) {
        queries[fill + j] = e164s[static_cast<int>(i + j)];
      }
      fill += take;
      i    += take;
      total_records_queried += take;

      // submit when full
      if (fill == batch_size) {
        auto flags = oram.FindBatch(queries.begin(), queries.begin() + fill, results.begin());
        total_db_batches += 1;
        (void)flags;
        fill = 0;
      }
    }
  }

  // flush tail
  if (fill > 0) {
    auto flags = oram.FindBatch(queries.begin(), queries.begin() + fill, results.begin());
    total_db_batches += 1;
    (void)flags;
  }

  uint64_t end_query_time = current_time_ns();

  int memAfter = getMemValue();

  double total_ns       = (double)(end_query_time - start_query_time);
  double avg_ns_record  = total_records_queried ? total_ns / total_records_queried : 0.0;
  double avg_ns_batch   = total_db_batches ? total_ns / total_db_batches : 0.0;
  double avg_ns_query = Q ? total_ns / Q : 0.0;

  double rec_rps = avg_ns_record  > 0.0 ? 1e9 / avg_ns_record  : 0.0; // records/sec
  double qry_qps = avg_ns_batch   > 0.0 ? 1e9 / avg_ns_batch   : 0.0; // queries/sec
  double log_qps = avg_ns_query > 0.0 ? 1e9 / avg_ns_query : 0.0; // logical queries/sec

  REPORT_LINE(
    "RWSignal", "olabs_oram_sharded",
    "N:=%zu | Key_bytes := 8 | Value_bytes := 56 | Shards := %d | Batch_size := %zu | "
    "fill:=0.8 | Initialization_time_us := %.2f | "
    "Record_latency_us := %.2f | Record_throughput_rps := %.2f | "
    "Batch_latency_us := %.2f | Batch_throughput_qps := %.2f | "
    "Query_latency_us := %.2f | Query_throughput_qps := %.2f | "
    "Memory_kb := %d",
    (size_t)N, (int)NUM_SHARDS, batch_size,
    (end_ns_create - start_ns_create) / 1000.0,
    avg_ns_record / 1000.0, rec_rps,
    avg_ns_batch  / 1000.0, qry_qps,
    avg_ns_query / 1000.0, log_qps,
    memAfter - memBefore
  );

  return 0;
}


int main() {
  for (uint64_t &N : std::array<uint64_t, 4> {10000000, 20000000, 50000000, 100000000}) {
    for (uint64_t &B : std::array<uint64_t, 4> {100,1000,4096,8192}) {
      RUN_TEST_FORKED((benchmark_umap_sharded(N, 10000, B)));
    }
  }

  // UNDONE(): Add 8bk8bv?
  return 0;
}