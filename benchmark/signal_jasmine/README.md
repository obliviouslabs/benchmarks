Signal Jasmine benchmarks the current `ContactDiscoveryService-Icelake` code after
Signal moved the ORAM/ohtable internals to Jasmin-generated code.

Signal-icelake has the following implementations that we want to benchmark:

+ c/path_oram -> non-recursive path oram

+ c/position_map -> recursive position map

+ c/ohtable -> non sharded hashtable (benchmarks/tests/loaded_table.c)

+ c/sharded_ohtable -> sharded hashtable (benchmarks/tests/loaded_sharded_table.c)


The goal of this project is to benchmark the newer Jasmin-backed implementation
next to the pinned pre-Jasmin `signal_icelake` benchmark.

Unlike the pre-Jasmin API, the Jasmin-backed constructors do not accept per-run
capacity and stash-size parameters. Capacity is selected by the upstream Jasmin
parameter set compiled into the test libraries. Setup clones upstream and its
submodules once, then prepares independent trees under
`build/signal_jasmine/L10/`, `L11/`, ..., `L24/` with 16 shards. `build.sh`
compiles each prepared depth once, before benchmarking. Source copies use
copy-on-write where supported; keeping all depths requires more disk space.

`SIGNAL_JASMINE_PATH_LENGTHS` controls the prepared sweep (default: 10 through
24). `SIGNAL_JASMINE_PATH_LENGTH` selects the ramp depth (default: 16),
which setup also prepares if it is not in the sweep. Both accept depths 8–30.
All variants use the same upstream revision, selected by `SIGNAL_JASMINE_REF`.

For example, prepare and build a smaller sweep, then reuse it:

```sh
SIGNAL_JASMINE_PATH_LENGTHS="10 12 14 16 18 20" sh scripts/setup.sh signal_jasmine
sh benchmark/signal_jasmine/run.sh
sh benchmark/signal_jasmine/run.sh SWAP512M
```

The last command labels the run; apply memory limits externally as in `go.sh`.
`run.sh` uses the sweep saved by setup, or a runtime
`SIGNAL_JASMINE_PATH_LENGTHS` override to select already-built depths. It never
clones, resets, or compiles. A missing binary fails immediately with a build
instruction, rather than rebuilding inside a memory-limited run. Normal, SWAP,
plan, and selector-based runs all use the same binaries.

The runner writes per-depth logs such as `path_oram_L16.out`,
`loaded_table_L16.out`, and `loaded_sharded_table_L16.out` under one log directory,
and appends all parsed rows directly to one results file. Rows include
`Path_length` so the compiled maximum ORAM size is separate from `N`.

`go.sh` builds at the beginning and reuses those binaries for ramp testing too.
It still resets builds at the start of each complete invocation; this is not a
persistent cache across separate `go.sh` runs. After changing benchmark code or
helpers, rerun setup and build to refresh every depth.

Longer point-size sweeps can be enabled by setting `BENCHMARK_TEST_TIMEOUT_MS`
(milliseconds) before running `run.sh`. The default in these scripts is 4h (`14400000`).

When more than one path length can run the same benchmark row, the raw aggregate
results file keeps all rows. The plotting/table loader in `scripts/utils.py`
chooses only the smallest working `Path_length` for each `N` and benchmark shape,
so figures do not average together multiple compiled maximum ORAM sizes.

To run one already-built depth, use the same runner:

```sh
SIGNAL_JASMINE_PATH_LENGTHS=16 sh benchmark/signal_jasmine/run.sh
```

The ramp driver uses setup's ramp depth, or `SIGNAL_JASMINE_PATH_LENGTH` to
select another already-built depth. The `build/signal_jasmine/c` symlink points
to setup's ramp-depth `L*/c` directory for callers using the old binary path.

The regular `path_oram.test` default matches `signal_icelake`: it sweeps
database sizes `N = 1 << 10` through `1 << 28` and skips rows that exceed the
compiled Jasmine capacity for the current path length. Set
`SIGNAL_JASMINE_PATH_ORAM_ARGS=capacity` only when you explicitly want one row at
the compiled maximum capacity.

We want to benchmark these things:

+ Initialization time - how fast can a server be initialized

+ Query time - What is the latency of a point query

+ Batched query time - What is the latency of a batched query (only relevant for sharded)

+ Insertion time - What is the latency of a point insertion

+ Batched insertion time - What is the latency of a batched insertion
