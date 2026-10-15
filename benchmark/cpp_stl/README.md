# C++ STL baseline

**Non-oblivious, single-threaded** reference measurements, not ORAM/OMAP
implementations. `std::vector` supplies a contiguous indexed array for the existing
`RORAM` report category; `std::unordered_map` supplies the `UnorderedMap` category.
Both use implementation name `cpp_stl` (labeled non-oblivious in README tables).

## Workload

For each N, with 8-byte keys/indices and 8- or 56-byte values:

1. Prepare distinct keys, key-dependent payloads, and a shuffled insertion order
   outside timing. Arrays use indices 0…N−1; maps use a bijective 64-bit mixing
   function so the usual identity integer hash does not get a collision-free
   consecutive-key workload. Shuffling uses a fixed seed.
2. Allocate/zero the array, or set `max_load_factor(0.8f)` and **then** `reserve(N)`
   on the empty map. This setup is timed separately.
3. Time exactly **N insertions** (indexed assignments for the array, `emplace`
   for the map). Map node allocation and payload copying are included. Verify
   that all insertions succeeded and that the reserved map did not rehash.
4. Reshuffle the populated keys outside timing, then time exactly **N successful
   queries**, visiting every key once. Each query reads the entire payload and
   adds its first word to a checksum. Check that checksum and every stored
   payload after timing; no correctness assertions disappear under `NDEBUG`.

The maximum load factor **0.8** targets at most about 0.8 entries per bucket,
a moderate memory/collision tradeoff. It is not necessarily the attained load
factor: STL implementations round bucket counts. Reports include `Max_load_factor`,
actual `Load_factor`, and `Bucket_count`; actual load factor is `N / Bucket_count`.
The default hasher, equality, allocator, and the compiler's default STL are used.
Compiler and standard-library versions are recorded in each report.

Timing brackets whole loops, not individual calls, avoiding a clock call on each
nanosecond-scale operation. Average insertion/query latency is phase time / N;
throughput is N / phase time. Compiler barriers preserve stores and register-input
`do_not_optimize` markers consume **every** queried payload word. The small loop,
marker, hit-check (map), and checksum overheads remain included; these are not
isolated instruction latencies. Timed kernels are `noinline` for easy inspection,
adding one wrapper call per phase; STL internals may still make calls per access.

Reports contain `Insertion_latency_us`, `Insertion_throughput_qps`, and either
`Read_latency_us` / `Read_throughput_qps` (array) or `Get_latency_us` /
`Get_throughput_qps` (map), plus operation counts and setup time. Latencies retain
six decimal places in microseconds; tiny values also retain precision in Markdown
rather than rounding to zero. `Memory_kb` is live RSS growth after input preparation,
not peak RSS; input buffers themselves are excluded from that delta.
This fully populated shuffled workload differs from existing hot-key sweeps.

## Running

Requires Linux, GCC or Clang with C++20, and uv. No downloads or external benchmark
library are needed. The sweep in `benchmark_code/stl_bench.cpp` covers powers of
two from 2¹⁰ through 2²⁸; large cases need tens of GiB including input buffers.
Use the shared selectors to limit a smoke run if necessary.

```sh
sh scripts/setup.sh cpp_stl
sh benchmark/cpp_stl/run.sh
# Or build with Clang:
CXX=clang++ sh scripts/setup.sh cpp_stl
```

Also wired into `go.sh`, with the usual planning, selectors, lifecycle JSONL,
timeouts, and parsed result files. Each case has N timed insertions and N timed
queries; preparation, validation, and cleanup add to the whole-case elapsed time.
The result filename uses a SHA-256 fingerprint of the copied local sources, so
uncommitted benchmark changes do not get mislabeled as an old Git revision.

For a plan without running any cases (after setup):

```sh
BENCHMARK_PROJECT=cpp_stl BENCHMARK_MODE=plan \
  build/cpp_stl/cpp_stl_bench > /tmp/cpp-stl-plan.jsonl
uv run scripts/run.py --cases /tmp/cpp-stl-plan.jsonl > /tmp/cpp-stl-cases.txt
# Edit /tmp/cpp-stl-cases.txt, leaving only small cases enabled, then:
BENCHMARK_SELECTOR=/tmp/cpp-stl-cases.txt sh benchmark/cpp_stl/run.sh
```

The release build uses `-O3 -DNDEBUG -march=native -g`. Inspect the optimized
`insert_array`, `query_array`, `insert_map`, and `query_map` specializations with:

```sh
objdump -d -C build/cpp_stl/cpp_stl_bench | less
```

Queries must retain key loads, indexed array loads or hash/bucket-chain traversal,
all payload-word loads, checksum accumulation, and an N-iteration back edge.
Insertions must retain payload stores and (for the map) node allocation/linking.
The empty-asm markers themselves emit no instructions or hardware fences.
