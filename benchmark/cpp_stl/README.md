# C++ STL baseline

**Non-oblivious, single-threaded** reference measurements, not ORAM/OMAP
implementations. `std::vector` supplies a contiguous indexed array for the existing
`RORAM` report category; `std::unordered_map` supplies the `UnorderedMap` category.
Both use implementation name `cpp_stl` (labeled non-oblivious in README tables).

## Workload

For each N, with 8-byte keys/indices and 8- or 56-byte values, run the following
workload **twice**: first for pipelined throughput, then for serialized latency.
Each run uses a fresh container and the same deterministic inputs:

1. Prepare distinct keys, key-dependent payloads, and a shuffled insertion order
   outside timing. Arrays use indices 0…N−1; maps use a bijective 64-bit mixing
   function so the usual identity integer hash does not get a collision-free
   consecutive-key workload. Shuffling uses a fixed seed.
2. Allocate/zero the array, or set `max_load_factor(1.0f)` and **then** `reserve(N)`
   on the empty map. This setup is timed separately.
3. Time exactly **N insertions** (indexed assignments for the array, `emplace`
   for the map). Map node allocation and payload copying are included. Verify
   that all insertions succeeded and that the reserved map did not rehash.
4. Reshuffle the populated keys outside timing, then time exactly **N successful
   queries**, visiting every key once. Each query reads the entire payload and
   adds its first word to a checksum. Check that checksum and every stored
   payload after timing; no correctness assertions disappear under `NDEBUG`.

The current maximum load factor **1.0** targets at most one entry per bucket on
average (the standard default). It is not necessarily the attained load factor:
STL implementations round bucket counts. Reports include `Max_load_factor`,
actual `Load_factor`, and `Bucket_count`; actual load factor is `N / Bucket_count`.
The default hasher, equality, allocator, and the compiler's default STL are used.
Compiler and standard-library versions are recorded in each report.

## Timing modes

The kernels and benchmark functions take a boolean non-type template parameter,
`Serialized`. `iteration_barrier<Serialized>()` uses `if constexpr`, so there is
no runtime mode branch inside the measured loops:

- **`false`: pipelined throughput.** Compiler-only memory barriers preserve stores;
  `do_not_optimize` markers consume every queried payload word and the checksum.
  No per-iteration hardware fence prevents CPU overlap. Report N / phase time.
- **`true`: serialized latency.** Keep those payload/checksum markers and end every
  successful insertion/query with `CPUID`, completing prior work, including stores,
  before the next iteration's access executes. Report phase time / N.

Timing brackets whole loops, not individual calls. **Both modes retain identical
`CPUID` clock-boundary barriers**; only the per-iteration barrier changes. Caches
are not flushed. **Barrier overhead is included, not subtracted**, along with
loop, hit-check (map), and checksum overhead. Serialized latency is an average
operation-plus-barrier time, not raw STL latency or a latency percentile; `CPUID`
can dominate small operations. Pipelined throughput is not its reciprocal.
Timed kernels are `noinline` for inspection, adding one wrapper call per phase;
STL internals may still make calls per access.

Each mode emits its own report with exact operation counts and provenance:

| Mode | Report metadata | Performance fields |
|---|---|---|
| Throughput | `Timing_mode := pipelined`, `Iteration_barrier := compiler` | `Insertion_throughput_qps`, `Read_throughput_qps` (array) or `Get_throughput_qps` (map) |
| Latency | `Timing_mode := serialized`, `Iteration_barrier := cpuid` | `Insertion_latency_us`, `Read_latency_us` (array) or `Get_latency_us` (map) |

Only the intended metric family is emitted in each report. Existing tables and
figures therefore use the correct mode without averaging or deriving reciprocal
metrics; skipping/failing one mode leaves its measurements missing. Do not mix
these modes with older single-mode snapshots.

Generic setup/memory fields (`Initialization_time_us`, array
`Initialization_zeroed_time_us`, `Memory_kb`) come from the pipelined run only,
so plots do not average two runs. The serialized report retains its diagnostics
as `Serialized_initialization_time_us` and `Serialized_memory_kb`. Memory is live
RSS growth after input preparation, not peak RSS; input buffers are excluded.
Latencies retain six decimal places in microseconds; tiny values retain precision
in Markdown rather than rounding to zero. This fully populated shuffled workload
differs from existing hot-key sweeps.

## Running

Requires Linux on x86-64, GCC or Clang with C++20, and uv. No downloads or external
benchmark library are needed. The sweep in `benchmark_code/stl_bench.cpp` covers powers of
two from 2¹⁰ through 2²⁸; large cases need tens of GiB including input buffers.
Use the shared selectors to limit a smoke run if necessary.

```sh
sh scripts/setup.sh cpp_stl
sh benchmark/cpp_stl/run.sh
# Or build with Clang:
CXX=clang++ sh scripts/setup.sh cpp_stl
```

Also wired into `go.sh`, with the usual planning, selectors, lifecycle JSONL,
timeouts, and parsed result files. The two modes are separate, independently
selectable cases, each with N timed insertions and N timed queries; the pair does
2N of each. Preparation, validation, and cleanup add to whole-case elapsed time.
Modes execute sequentially in separate child processes, not on simultaneously
allocated containers.
The result filename uses a SHA-256 fingerprint of the copied local sources, so
uncommitted benchmark changes do not get mislabeled as an old Git revision.

Rebuild and regenerate existing selectors: template arguments and case parameters
now distinguish `timing_mode=pipelined,iteration_barrier=compiler` from
`timing_mode=serialized,iteration_barrier=cpuid`. Enable both modes for each wanted
configuration to obtain both metrics; old selectors will not silently select them.
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
The `<..., false>` kernels must contain no hardware fences; `<..., true>` must
execute `cpuid` after each successful iteration's stores or payload loads and
checksum, before the next access. Neither kernel may contain per-access clock
calls or runtime mode dispatch. Empty-asm compiler markers emit no instructions;
`serialize_cpu()` is the separate hardware barrier.
