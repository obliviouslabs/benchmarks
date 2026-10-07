# SONIC

Evaluates native SONIC NRORAM and PMCHAIN OMAP from Nihal Talur and Ioannis
Demertzis's [USENIX Security 2026 paper](https://www.usenix.org/conference/usenixsecurity26/presentation/talur).
Setup downloads and checksum-verifies the [official artifact](https://zenodo.org/records/21522021)
(CC BY 4.0). These are **non-SGX** measurements, not a reproduction of enclave timings.

## Benchmarks

- **NRORAM:** SONIC RingORAM (`zingoram`) with caller-managed leaf positions.
  Sequential accesses (`Ser`) or parallel accesses with 8, 16, or 32 threads
  (`Par8`, `Par16`, `Par32`). All configurations use eight eviction workers.
- **OMAP:** PMCHAIN, combining an O2TH position map with SONIC. Uses eight workers
  and batches of 1K, 4K, 8K, 64K, or 1M queries (powers of two, with batch size ≤ N).

These sweeps use 8-byte keys/addresses and 8- or 56-byte values across a range of sizes.
The independent sweeps are defined in [`nroram_bench.cpp`](benchmark_code/nroram_bench.cpp)
and [`pmchain_bench.cpp`](benchmark_code/pmchain_bench.cpp), built as
`sonic_nroram_bench` and `sonic_pmchain_bench` under `build/sonic/build/bin/`.

Measures initialization time, read latency, throughput, and RSS growth. Timings
include eviction; OMAP also includes position-map processing and sorting.
NRORAM latency is per access, while OMAP latency is per complete batch.

The sweeps read prewritten hot keys, with every payload checked: one key per access
worker for NRORAM, or one batch of keys for OMAP. This is not a fully populated,
random-key workload.

**Ramp OMAP** ([`sonic_pmchain_ramp`](benchmark_code/pmchain_ramp.cpp)) instead
preloads all N keys and cycles through them, using 8-byte values and eight workers.
It supports the shared ramp, constant, and intermittent workloads, checking every read.
`RAMP_MAX_BATCH_SIZE` caps queue batches (default: largest power of two ≤ min(N, 64K)).
PMCHAIN pads these to a fixed power-of-two capacity of at least 64, which must fit N
(N ≥ 2048). Per-request latency includes queueing, padding, position-map processing,
and eviction; CSV batch sizes count only real requests. Large N can make preloading slow.

Upstream parallel bucket rebuilding shares a non-thread-safe PRNG (confirmed with
ThreadSanitizer). This remains unpatched; treat parallel results as provisional.

## Running

Requires Linux, Clang with C++20 support, CMake, Ninja, OpenSSL development headers,
curl, and uv. No SGX SDK, Docker, or MPI is needed.

```sh
sh scripts/setup.sh sonic
sh benchmark/sonic/run.sh # Runs both fixed-workload binaries and combines their reports.
RAMP_IMPLEMENTATIONS=sonic RAMP_MAP_SIZES=65536 sh scripts/run_ramp_latency.sh
```

Also included in `go.sh`. Use the shared [case selectors](../../README.md#running-benchmarks)
to choose sizes and configurations; large sizes can require substantial RAM and time.
