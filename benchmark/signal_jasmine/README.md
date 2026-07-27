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
parameter set compiled into the test libraries. The setup script defaults to the
test ORAM depth, switches the sharded test parameter to 16 shards, and accepts
`SIGNAL_JASMINE_PATH_LENGTH` to rebuild the host test binary with a different
maximum ORAM size.

Longer point-size sweeps can be enabled by setting `BENCHMARK_TEST_TIMEOUT_MS`
(milliseconds) before running `run.sh`. The default in these scripts is 4h (`14400000`).

Use `run.sh` to benchmark scaling across compiled maximum ORAM sizes:

```
SIGNAL_JASMINE_PATH_LENGTHS="10 12 14 16 18 20" sh benchmark/signal_jasmine/run.sh
```

That script rebuilds once per depth, writes per-depth logs such as
`path_oram_L16.out`, `loaded_table_L16.out`, and
`loaded_sharded_table_L16.out` under one log directory, and appends all parsed
rows to one results file. Rows include `Path_length` so the compiled maximum
ORAM size is separate from `N`.

When more than one path length can run the same benchmark row, the raw aggregate
results file keeps all rows. The plotting/table loader in `scripts/utils.py`
chooses only the smallest working `Path_length` for each `N` and benchmark shape,
so figures do not average together multiple compiled maximum ORAM sizes.

Use `run_for_fixed_depth.sh` only when the Jasmine build has already been set up
for the depth you want to run.

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
