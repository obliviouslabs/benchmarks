#include "./common.h"
using namespace std;

#include "odsl/omap_short_kv.hpp"

#define private public
#include "odsl/par_omap.hpp"
#undef private

const uint64_t NUM_SHARDS = 15;

template <typename K, typename V, typename PositionType = uint64_t>
class LoadBalancingBenchmark {
  using ParOMapType = ODSL::ParOMap<K, V, PositionType>;
  using BaseMap = typename ParOMapType::BaseMap;
  using KeyInfo = typename ParOMapType::KeyInfo;

  uint8_t randSalt[16];
  uint64_t shardHashRange;
  uint64_t shardCount;
  public:
  LoadBalancingBenchmark(uint64_t _shardCount) {
    if (_shardCount == 0) {
      throw std::runtime_error("shardCount should be positive");
    }
    if (_shardCount > 64) {
      throw std::runtime_error("shardCount should be no more than 64");
    }
    shardCount = _shardCount;
    read_rand(randSalt, sizeof(randSalt));
    shardHashRange = (UINT64_MAX - 1) / shardCount + 1;
  }

  INLINE uint32_t getShardByHash(uint64_t hash) {
    return (uint32_t)(hash / shardHashRange);
  }

  template <class KeyIterator, class ValueIterator>
  std::vector<uint8_t> FindBatchDeferWriteBack(
    const KeyIterator keyBegin,
    const KeyIterator keyEnd,
    ValueIterator valueBegin) 
  {
    uint32_t batchSize = (uint32_t)(keyEnd - keyBegin);
    // Calculate the max unique element per shard with high probability.
    uint32_t batchSizePerShard = (uint32_t) ParOMapType::maxQueryPerShard(batchSize, shardCount);
    std::vector<KeyInfo> keyInfoVec(batchSize);
    // hash the keys and determine the shard index for each key
    for (uint32_t i = 0; i < batchSize; ++i) {
      keyInfoVec[i].key = *(keyBegin + i);
      keyInfoVec[i].hash = secure_hash_with_salt((uint8_t*)&keyInfoVec[i].key,
                                                 sizeof(K), randSalt);
      keyInfoVec[i].shardIdx = getShardByHash(keyInfoVec[i].hash);
    }

    // Parallel bitonic sort the input batch first by their hash, and if
    // there's a collision, by the key (for obliviousness, always perform two
    // comparisons). Sort together an index array as payload for recovery.
    std::vector<uint32_t> recoveryArr(batchSize);
    for (uint32_t i = 0; i < batchSize; ++i) {
      recoveryArr[i] = i;
    }
    Algorithm::ParBitonicSortSepPayload(keyInfoVec.begin(), keyInfoVec.end(),
                                        recoveryArr.begin(),
                                        (int)shardCount * 2);

    // Obliviously count the number of unique keys for each shard (using
    // simd acceleration). Also generate a prefix sum of the number of unique
    // elements.
    std::vector<uint32_t> shardLoads(shardCount, 0);
    std::vector<uint32_t> prefixSumFirstCompaction(batchSize + 1);
    prefixSumFirstCompaction[0] = 0;
    prefixSumFirstCompaction[1] = 1;
    for (uint32_t j = 0; j < shardCount; ++j) {
      bool matchFlag = keyInfoVec[0].shardIdx == j;
      shardLoads[j] += matchFlag;
    }
    for (uint32_t i = 1; i < batchSize; ++i) {
      bool isDup = keyInfoVec[i].key == keyInfoVec[i - 1].key;
      prefixSumFirstCompaction[i + 1] = prefixSumFirstCompaction[i] + !isDup;
#pragma omp simd
      for (uint32_t j = 0; j < shardCount; ++j) {
        bool matchFlag = (keyInfoVec[i].shardIdx == j) & (!isDup);
        shardLoads[j] += matchFlag;
      }
    }
    // With negligible probability, we need to enlarge the shard size for the
    // batch to avoid overflow
    for (uint32_t load : shardLoads) {
      obliMove(load > batchSizePerShard, batchSizePerShard, load);
    }

    std::vector<K> keyVec(shardCount * batchSizePerShard);
    for (uint32_t i = 0; i < batchSize; ++i) {
      keyVec[i] = keyInfoVec[i].key;
    }
    // Compact the unique elements in the sorted array using the prefix
    // sum.
    Algorithm::OrCompactSeparateMark(keyVec.begin(), keyVec.begin() + batchSize,
                                     prefixSumFirstCompaction.begin());
    std::vector<uint32_t> prefixSumSecondCompaction(
        shardCount * batchSizePerShard + 1);
    std::vector<uint32_t> shardLoadPrefixSum(shardCount);
    shardLoadPrefixSum[0] = 0;
    for (uint32_t i = 0; i < shardCount - 1; ++i) {
      shardLoadPrefixSum[i + 1] = shardLoadPrefixSum[i] + shardLoads[i];
    }
    prefixSumSecondCompaction[0] = 0;
    for (uint32_t i = 0; i < shardCount; ++i) {
      for (uint32_t j = 0; j < batchSizePerShard; ++j) {
        uint32_t rankInShard = j + 1;
        obliMove(rankInShard > shardLoads[i], rankInShard, shardLoads[i]);
        prefixSumSecondCompaction[i * batchSizePerShard + j + 1] =
            shardLoadPrefixSum[i] + rankInShard;
      }
    }
    // Distribute the compacted elements so that the elements of shard i
    // starts at offset shard_size * i.
    Algorithm::OrDistributeSeparateMark(keyVec.begin(), keyVec.end(),
                                        prefixSumSecondCompaction.begin());
    using ValResult = BaseMap::ValResult;
    std::vector<ValResult> resultVec(shardCount * batchSizePerShard);

    // Parallel query each shard, and get the result values (along with flags
    // of existence).
    // Commented out since we don't want to measure the time of queries here.
    // #pragma omp parallel for schedule(static)
    //     for (uint32_t i = 0; i < shardCount; ++i) {
    //       shards[i].FindBatchDeferWriteBack(
    //           keyVec.begin() + i * batchSizePerShard,
    //           keyVec.begin() + (i + 1) * batchSizePerShard,
    //           resultVec.begin() + i * batchSizePerShard);
    //     }

    // Compact the result values in reverse order of the previous
    // distribution.
    Algorithm::OrCompactSeparateMark(resultVec.begin(), resultVec.end(),
                                     prefixSumSecondCompaction.begin());

    // Distribute the compacted result values in reverse order of the previous
    // compaction.
    Algorithm::OrDistributeSeparateMark(resultVec.begin(),
                                        resultVec.begin() + batchSize,
                                        prefixSumFirstCompaction.begin());

    // Propagate the values of the duplicate elements.
    for (uint32_t i = 1; i < batchSize; ++i) {
      bool isDup = keyInfoVec[i].key == keyInfoVec[i - 1].key;
      obliMove(isDup, resultVec[i], resultVec[i - 1]);
    }

    // Parallel bitonic sort the values in reverse order the previous bitonic
    // sort using the recovery array
    Algorithm::ParBitonicSortSepPayload(recoveryArr.begin(), recoveryArr.end(),
                                        resultVec.begin(),
                                        shardCount * 2);
    std::vector<uint8_t> foundFlags(batchSize);
    for (uint32_t i = 0; i < batchSize; ++i) {
      foundFlags[i] = resultVec[i].found;
      *(valueBegin + i) = resultVec[i].value;
    }
    return foundFlags;
  }

  template <class KeyIterator, class ValueIterator>
  std::vector<uint8_t> FindBatchDeferWriteBack_Snoopy(
    const KeyIterator keyBegin,
    const KeyIterator keyEnd,
    ValueIterator valueBegin) 
  {
    uint32_t batchSize = (uint32_t)(keyEnd - keyBegin);
    // Calculate the max unique element per shard with high probability.
    uint32_t batchSizePerShard = (uint32_t) ParOMapType::maxQueryPerShard(batchSize, shardCount);
    std::vector<KeyInfo> keyInfoVec(shardCount * batchSizePerShard);

    std::vector<uint64_t> counts(shardCount, 0);
    uint64_t total_counts = 0;
    // hash the keys and determine the shard index for each key
    for (uint32_t i = 0; i < batchSize; ++i) {
      keyInfoVec[i].key = *(keyBegin + i);
      keyInfoVec[i].hash = secure_hash_with_salt((uint8_t*)&keyInfoVec[i].key,
                                                  sizeof(K), randSalt);
      keyInfoVec[i].shardIdx = getShardByHash(keyInfoVec[i].hash);
    }

    for (uint64_t i=0; i<batchSize; i++) {
      for (uint64_t j=0; j<shardCount; j++) {
        uint64_t is_in_shard = (keyInfoVec[i].shardIdx == j);
        obliMove(is_in_shard, total_counts, total_counts + 1);
        obliMove(is_in_shard, counts[j], counts[j] + 1);
      }
    }

    for (uint64_t i=batchSize; i<shardCount * batchSizePerShard; i++) {
      bool available = true;
      for (uint32_t j=0; j<shardCount; j++) {
        bool use = available * (counts[j] > 0);
        obliMove(use, keyInfoVec[i].shardIdx, j);
        obliMove(use, counts[j], counts[j] - 1);
        obliMove(use, available, false);
      }
    }
    
    std::vector<uint32_t> recoveryArr(shardCount * batchSizePerShard);
    for (uint32_t i = 0; i < shardCount * batchSizePerShard; ++i) {
      recoveryArr[i] = i;
    }
      
    Algorithm::ParBitonicSortSepPayload(keyInfoVec.begin(), keyInfoVec.end(),
                                        recoveryArr.begin(),
                                        (int)shardCount * 2);
    std::vector<uint32_t> compactionPrefixSum(shardCount * batchSizePerShard + 1);
    compactionPrefixSum[0] = 0;
    for (uint32_t i = 0; i < shardCount * batchSizePerShard; ++i) {
      compactionPrefixSum[i + 1] = compactionPrefixSum[i] + 1;
      bool isDummy = keyInfoVec[i].hash == 0;
      obliMove(isDummy, compactionPrefixSum[i + 1], compactionPrefixSum[i]); 
    }
    Algorithm::OrCompactSeparateMark(keyInfoVec.begin(), keyInfoVec.end(), 
                                     compactionPrefixSum.begin());

    // At this point we would perform the queries to the shards, but we skip that
    // since we don't want to measure the time of queries here.

    keyInfoVec.reserve(keyInfoVec.size() + batchSize); // to avoid OOB
    for (uint32_t i = 0; i < batchSize; ++i) {
      KeyInfo k;
      k.key = *(keyBegin + i);
      k.hash = secure_hash_with_salt((uint8_t*)&k.key,
                                                 sizeof(K), randSalt);
      k.shardIdx = getShardByHash(k.hash);
      keyInfoVec.push_back(k);
      recoveryArr.push_back(i + shardCount * batchSizePerShard);
    }
    
    Algorithm::ParBitonicSortSepPayload(keyInfoVec.begin(), keyInfoVec.end(),
                                        recoveryArr.begin(),
                                        (int)shardCount * 2);


    // We are skipping the cost of the linear scan here, as it is negligible.
    for (uint32_t i = 0; i < batchSize; ++i) {
      compactionPrefixSum.push_back(compactionPrefixSum.back() + 1);
    }
    Algorithm::OrCompactSeparateMark(keyInfoVec.begin(), keyInfoVec.end(), 
                                     compactionPrefixSum.begin());

    return std::vector<uint8_t>(batchSize, 0); // Dummy found flags
  }
};




// UNDONE(): benchmark oblivious load balancing to compare with snoopy
template<typename K, typename V>
int benchmark_load_balancing(uint64_t B, uint64_t p) {
  uint64_t repetitions = 1000;

  std::vector<K> keys(B);
  std::vector<V> values(B);
  for (uint64_t i = 0; i < B; ++i) {
    keys[i] = (K)i;
  }
  LoadBalancingBenchmark<K,V> lbm(p);

  uint64_t start_ns = current_time_ns();  

  for (uint64_t rep = 0; rep < repetitions; ++rep) {
    lbm.FindBatchDeferWriteBack(
      keys.begin(),
      keys.end(),
      values.begin());
  }

  uint64_t end_ns = current_time_ns(); 

  uint64_t start_snoopy_ns = current_time_ns();  

  for (uint64_t rep = 0; rep < repetitions; ++rep) {
    lbm.FindBatchDeferWriteBack_Snoopy(
      keys.begin(),
      keys.end(),
      values.begin());
  }

  uint64_t end_snoopy_ns = current_time_ns(); 



  uint64_t total_time_ns = end_ns - start_ns;
  uint64_t total_time_snoopy_ns = end_snoopy_ns - start_snoopy_ns;
  double avg_time_ns = (double)total_time_ns / (double)repetitions;
  double avg_time_snoopy_ns = (double)total_time_snoopy_ns / (double)repetitions;
  
  REPORT_LINE("LOADBALANCE", "olabs_oram", "B := %zu | Key_bytes := %zu | Value_bytes := %zu |  Latency_us := %.2f | Throughput_qps := %.2f", B, sizeof(K), sizeof(V), avg_time_ns / 1000.0, B * (1000000000.0 / avg_time_ns));

  REPORT_LINE("LOADBALANCE", "snoopy", "B := %zu | Key_bytes := %zu | Value_bytes := %zu |  Latency_us := %.2f | Throughput_qps := %.2f", B, sizeof(K), sizeof(V), avg_time_snoopy_ns / 1000.0, B * (1000000000.0 / avg_time_snoopy_ns));
  return 0;  
}


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

  uint64_t main_timer = 0;
  uint64_t writeback_timer = 0;
  uint64_t start_query_time = current_time_ns();
  std::vector<typename BytesHelper<KEY_SIZE>::type> queries(queries_batch_size);
  std::vector<typename BytesHelper<VAL_SIZE>::type> results(queries_batch_size);
  uint64_t curr = N - 1;
  for (uint64_t i=0; i<num_queries; i+=queries_batch_size) {    
    for (uint64_t j=0; j<queries_batch_size; j++) {
      queries[j] = BytesHelper<KEY_SIZE>::key_from_u64(curr);
      curr--;
    }
    uint64_t start_timer = current_time_ns();
    std::vector<uint8_t> findExistFlag =
      oram.FindBatchDeferWriteBack(queries.begin(), queries.end(), results.begin());
    uint64_t mid_timer = current_time_ns();
    oram.WriteBack();
    uint64_t end_timer = current_time_ns();
    main_timer += (mid_timer - start_timer);
    writeback_timer += (end_timer - mid_timer);
  }
  uint64_t end_query_time = current_time_ns();

  int memAfter = getMemValue();

  double avg_ns_success = (double)(end_query_time - start_query_time) / num_queries;
  double avg_ns_main = (double)main_timer / num_queries;
  double avg_ns_writeback = (double)writeback_timer / num_queries;

  REPORT_LINE("UnorderedMap_Burst", "olabs_oram_sharded", "N:=%zu | Key_bytes := %zu | Value_bytes := %zu | Shards := %d | Batch_size := %zu | fill:=0.8 | Initialization_time_us := %.2f | Get_latency_us := %.2f | Get_throughput_qps := %.2f | Get_burst_latency_us := %.2f | Get_writeback_latency_us := %.2f | Memory_kb := %d", N, KEY_SIZE, VAL_SIZE, NUM_SHARDS, batch_size, (end_ns_create - start_ns_create) / 1000.0, avg_ns_success / 1000.0 * batch_size, 1000000000.0 / avg_ns_success, avg_ns_main / 1000.0 * batch_size, avg_ns_writeback / 1000.0 * batch_size, memAfter - memBefore);

  return 0;
}

int main() {
  for (uint64_t i = 10; i<=20; i++) {
    RUN_TEST_FORKED( (benchmark_load_balancing<uint64_t,uint64_t>(1<<i, NUM_SHARDS)) );    
   // RUN_TEST_FORKED( (benchmark_load_balancing<uint64_t,Bytes<56>>(1<<i, NUM_SHARDS)) );
  }

  uint64_t batch_sizes[] = {NUM_SHARDS,1024,4096,8192,65536,1<<20};

  for (uint64_t i = 0; i<6; i++) {
    for (uint64_t j = 10; j<=20; j++) {
      RUN_TEST_FORKED((benchmark_umap_sharded<8,8>(1<<j, batch_sizes[i])));
      RUN_TEST_FORKED((benchmark_umap_sharded<8,56>(1<<j, batch_sizes[i])));
    }
  }

  return 0;
}