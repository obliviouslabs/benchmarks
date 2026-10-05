# Oblivious Algorithms Benchmarks

This repository contains reproducible benchmarks related to several implementations of ORAM, Oblivious Maps and other oblivious algorithms targeting TEEs (Trusted Execution Environments). To reproduce, simply run `sh go.sh`. Our goal with this repo is to provide transparent analysis of the performance of several oblivious algorithms, and to incentivize the development of efficient implementations of oblivious algorithms.

See [benchmark types](#benchmark-types) for information about the algorithms that are being benchmarked and implementations for each algorithm and [benchmark results](#benchmark-results) for a summary of the results of each implementation.
Contributions with new implementations are welcome, see [CONTRIBUTING.md](./CONTRIBUTING.md) for information on how to add new benchmarks.


## Benchmark Types

| Name | Description | Measured Parameters | Independent Variables |
| :-   | :-          | :-:                 | :- |
| NRORAM | Non recursive ORAM (Requires storing the position somewhere for accesses) | Read Latency, Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Address size), V (Value size) |
| RORAM | Recursive ORAM - Basically an array | Read Latency, Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Address size), V (Value size)
| UMAP | Unordered Map / Key-Value Store (Many disperse keys) | Read Latency, Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Key size), V (Value size)
| Sharded-UMAP | Sharded UMap | Batch Read Latency, Batch Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Key size), V (Value size), P (Number of Partitions), B (Queries per batch)
| Ramp-UMAP | Open-loop point lookups with ramp, constant, or intermittent input | End-to-end average and tail latency, raw arrival/response timestamps, dynamic caller batch sizes | N (map size), workload and input-rate controls |

## Benchmarked Implementations

| Name | Description | Language | Types |
| :-   | :-          | :-       | :-:   |
| [**olabs_oram**](https://github.com/obliviouslabs/oram) | Oblivious Labs's SGX implementation of ORAM and UMAP | C++ | NRORAM, RORAM, UMAP, Sharded-UMAP, Ramp-UMAP |
| [**signal_icelake**](https://github.com/signalapp/ContactDiscoveryService-Icelake) | Signal's pre-Jasmin implementation of ORAM and UMAP for Private Contact Discovery | C++ | UMAP, Sharded-UMAP, Ramp-UMAP |
| [**signal_jasmine**](https://github.com/signalapp/ContactDiscoveryService-Icelake) | Signal's Jasmin-backed implementation of ORAM and UMAP for Private Contact Discovery | C/Jasmin | UMAP, Sharded-UMAP, Ramp-UMAP |
| [**olabs_rostl**](https://github.com/obliviouslabs/rostl) | Oblivious Labs's Rust Oblivious Standard Library implementation of ORAM and UMAP | Rust |  NRORAM, RORAM, UMAP, Sharded-UMAP, Ramp-UMAP |
| [**mc_oblivious**](https://github.com/mobilecoinfoundation/mc-oblivious) | Mobilecoin's implementation of ORAM and UMAP | Rust | RORAM, UMAP, Ramp-UMAP |
| [**meta_oram**](https://github.com/facebook/oram) | Meta's implementation of RORAM | Rust | RORAM |
| [**h2o2_oram**](https://github.com/55199789/H2O2RAM) | H2O2RAM's high-performance hierarchical doubly oblivious RAM | C++ | RORAM, Ramp-UMAP |

## Benchmark Results

### Environment

All the benchmarks are ran on the same environment. A mini server with an AMD Ryzen 9 7945HX with 64GB of RAM, and an WD Black SN850 SSD, running an up to date version of arch linux.

### Non-Recursive ORAM (NRORAM)

# NRORAM - Latency

Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values 

| N | olabs_rostl | olabs_oram |
| :--- |  :---: | :---: |
| $2^{10}$ | **0.47** | 0.55 |
| $2^{11}$ | **0.51** | 0.60 |
| $2^{12}$ | **0.53** | 0.67 |
| $2^{13}$ | **0.56** | 0.69 |
| $2^{14}$ | **0.61** | 0.74 |
| $2^{15}$ | **0.65** | 0.77 |
| $2^{16}$ | **0.70** | 0.82 |
| $2^{17}$ | **0.75** | 0.83 |
| $2^{18}$ | **0.80** | 0.87 |
| $2^{19}$ | **0.88** | 0.91 |
| $2^{20}$ | **0.99** | 1.02 |
| $2^{21}$ | **1.02** | 1.03 |
| $2^{22}$ | 1.10 | **1.08** |
| $2^{23}$ | 1.16 | **1.08** |
| $2^{24}$ | 1.30 | **1.16** |
| $2^{25}$ | 1.49 | **1.31** |
| $2^{26}$ | 1.71 | **1.44** |
| $2^{27}$ | 1.98 | **1.55** |
| $2^{28}$ | 2.24 | **1.78** |

### Recursive ORAM (RORAM)

# RORAM - Latency

Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values 

| N | meta_oram | olabs_oram | olabs_rostl |
| :--- |  :---: | :---: | :---: |
| $2^{10}$ | 31.99 | **0.32** | 0.54 |
| $2^{11}$ | 36.34 | **0.38** | 0.62 |
| $2^{12}$ | 42.41 | **0.91** | 1.32 |
| $2^{13}$ | 51.34 | **0.96** | 1.32 |
| $2^{14}$ | 66.21 | **1.13** | 1.38 |
| $2^{15}$ | 91.63 | 1.81 | **1.40** |
| $2^{16}$ | 139.80 | **2.01** | 2.40 |
| $2^{17}$ | 229.08 | 2.40 | **2.39** |
| $2^{18}$ | 149.55 | 3.18 | **2.50** |
| $2^{19}$ | 201.49 | 3.70 | **2.60** |
| $2^{20}$ | 296.34 | 4.21 | **4.20** |
| $2^{21}$ | 219.94 | 5.22 | **4.25** |
| $2^{22}$ | 276.29 | 5.85 | **4.34** |
| $2^{23}$ | 376.70 | 6.79 | **4.33** |
| $2^{24}$ | 304.79 | 7.94 | **6.58** |
| $2^{25}$ | 367.06 | 8.68 | **6.73** |
| $2^{26}$ | 470.23 | 9.59 | **7.04** |
| $2^{27}$ |  | 11.05 | **7.16** |
| $2^{28}$ |  | **12.26** |  |

### Unordered Map (UMAP)

#### Single Threaded

Latency (us) vs Map number of entries (N), using 8 byte keys, 56 byte values 

| N | Signal | olabs_rostl | olabs_umap_shortkv | olabs_umap |
| :--- |  :---: | :---: | :---: | :---: |
| $2^{10}$ | 7.65 | 2.07 | 3.44 | **1.51** |
| $2^{11}$ | 8.81 | 2.28 | 2.18 | **2.11** |
| $2^{12}$ | 10.57 | 2.50 | **2.36** | 2.53 |
| $2^{13}$ | 12.96 | 4.07 | 2.76 | **2.62** |
| $2^{14}$ | 14.75 | 4.30 | 3.29 | **3.04** |
| $2^{15}$ | 16.61 | 4.50 | 3.68 | **3.61** |
| $2^{16}$ | 19.36 | 5.22 | 4.07 | **4.11** |
| $2^{17}$ | 24.37 | 7.91 | 4.57 | **4.39** |
| $2^{18}$ | 33.81 | 8.60 | 5.53 | **5.02** |
| $2^{19}$ | 33.96 | 9.01 | 6.17 | **5.90** |
| $2^{20}$ | 38.28 | 9.48 | 6.94 | **6.77** |
| $2^{21}$ | 43.11 | 13.20 | 7.59 | **7.38** |
| $2^{22}$ | 48.77 | 13.89 | 8.62 | **8.13** |
| $2^{23}$ | 56.38 | 14.51 | 9.53 | **9.03** |
| $2^{24}$ | 65.83 | 15.24 | 10.30 | **10.04** |
| $2^{25}$ | 81.48 | 20.29 | 11.29 | **11.07** |
| $2^{26}$ |  | 21.27 | **12.30** |  |


Latency (us) vs Map number of entries (N), using 8 byte keys, 8 byte values 

| N | mc_oblivious | Signal | olabs_rostl | olabs_umap_shortkv | olabs_umap |
| :--- |  :---: | :---: | :---: | :---: | :---: |
| $2^{10}$ | 80.37 | 5.45 | **1.29** | 1.58 | 1.45 |
| $2^{11}$ | 99.40 | 6.28 | **1.43** | 1.82 | 2.05 |
| $2^{12}$ | 120.18 | 7.98 | **1.62** | 2.24 | 2.37 |
| $2^{13}$ | 142.36 | 8.92 | 3.06 | **2.51** | 2.58 |
| $2^{14}$ | 167.51 | 11.10 | 3.17 | 3.63 | **3.01** |
| $2^{15}$ | 195.07 | 12.50 | **3.30** | 4.08 | 3.44 |
| $2^{16}$ | 224.88 | 15.10 | **3.44** | 4.90 | 4.05 |
| $2^{17}$ | 307.50 | 16.47 | 6.20 | 5.74 | **4.33** |
| $2^{18}$ | 354.76 | 19.84 | 6.41 | 7.34 | **4.76** |
| $2^{19}$ | 404.10 | 25.06 | 6.74 | 8.11 | **5.60** |
| $2^{20}$ | 457.49 | 33.67 | 6.91 | 8.94 | **6.50** |
| $2^{21}$ | 514.50 | 34.65 | 10.24 | 10.18 | **7.16** |
| $2^{22}$ | 573.55 | 38.36 | 10.84 | 9.95 | **7.88** |
| $2^{23}$ | 637.82 | 43.17 | 11.27 | 12.39 | **8.85** |
| $2^{24}$ | 706.21 | 48.47 | 12.01 | 14.16 | **9.88** |
| $2^{25}$ | 828.20 | 55.93 | 16.56 | 16.35 | **10.80** |
| $2^{26}$ | 915.86 | 65.39 | **17.23** | 20.45 |  |


#### Multi-threaded with 15 partitions

###### OMAP - Sharded Latency
 Latency (us) vs Map number of entries (N) vs BatchSize, using 8 byte keys, 56 byte values, 15 Shards 

| N | olabs_umap_sharded-4096 | olabs_rostl_sharded-4096 | Signal_Sharded-4096 |
| :--- |  :---: | :---: | :---: |
| $2^{10}$ | 13749.73 | **2734.09** |  |
| $2^{11}$ | 6501.88 | **4365.58** |  |
| $2^{12}$ | 3318.85 | **3087.36** | 20643.84 |
| $2^{13}$ | **2983.10** | 3089.08 | 31498.24 |
| $2^{14}$ | 5902.74 | **4792.69** | 13639.68 |
| $2^{15}$ | **5772.58** |  | 15032.32 |
| $2^{16}$ | **5064.08** |  | 16302.08 |
| $2^{17}$ | **4149.23** | 6646.30 | 22446.08 |
| $2^{18}$ | **5302.63** | 6902.83 | 26583.04 |
| $2^{19}$ | **5458.54** | 7568.51 | 32399.36 |
| $2^{20}$ | **5738.74** | 9806.74 | 36659.20 |
| $2^{21}$ | **6368.66** |  | 43950.08 |
| $2^{22}$ | **6878.30** | 13159.35 | 48128.00 |
| $2^{23}$ | **8357.75** | 14957.24 | 64552.96 |
| $2^{24}$ | **8836.04** | 17268.20 | 65904.64 |
| $2^{25}$ | **9651.10** | 33917.48 | 84336.64 |
| $2^{26}$ | **10353.66** |  | 81551.36 |

###### OMAP - Sharded Throughput
 Througput (queries per second) vs Map number of entries (N), using 8 byte keys, 56 byte values, 15 Shards 

| N | olabs_umap_sharded-4096 | olabs_rostl_sharded-4096 | Signal_Sharded-4096 |
| :--- |  :---: | :---: | :---: |
| $2^{10}$ | 297896.72 | **1498119.61** |  |
| $2^{11}$ | 629971.25 | **938247.71** |  |
| $2^{12}$ | 1234160.95 | **1326697.91** | 198452.91 |
| $2^{13}$ | **1373069.90** | 1325960.58 | 130044.45 |
| $2^{14}$ | 693914.76 | **854634.36** | 300086.56 |
| $2^{15}$ | **709560.87** |  | 272384.73 |
| $2^{16}$ | **808834.46** |  | 251091.91 |
| $2^{17}$ | **987172.01** | 616283.04 | 182582.01 |
| $2^{18}$ | **772447.25** | 593379.67 | 154155.17 |
| $2^{19}$ | **750383.43** | 541189.95 | 126428.27 |
| $2^{20}$ | **713745.44** | 417672.05 | 111731.96 |
| $2^{21}$ | **643148.98** |  | 93213.90 |
| $2^{22}$ | **595496.22** | 311261.68 | 85082.81 |
| $2^{23}$ | **490083.88** | 273847.26 | 63448.79 |
| $2^{24}$ | **463556.08** | 237199.07 | 62168.17 |
| $2^{25}$ | **424407.51** | 120763.67 | 48556.98 |
| $2^{26}$ | **395608.99** | 102460.02 | 50216.89 |


## Reproducing Benchmarks

### Running Benchmarks
`go.sh` sets up and runs the benchmarks, saving full-run JSONL logs under `logs/full_runs/`:
```sh
sh go.sh
```

`scripts/run.py` wraps `go.sh` or parses its saved full-run logs into editable case
lists or per-test timing/outcome CSV:

```sh
# List cases without running test bodies (setup/build still run):
BENCHMARK_MODE=plan uv run scripts/run.py > cases.txt
# Edit cases.txt: only run/yes entries execute; comment out cases to skip them.
BENCHMARK_SELECTOR=cases.txt uv run scripts/run.py > results.csv
# Run all encountered cases:
uv run scripts/run.py > results.csv
# Parse a saved run without executing anything:
uv run scripts/run.py logs/full_runs/FILE.jsonl > results.csv
# Generate a case list from a saved run:
uv run scripts/run.py --cases logs/full_runs/FILE.jsonl > cases.txt
```

### Generating Figures and tables

To generate tables from benchmark data, like the ones in the `README.md`:
```sh
uv run scripts/generate_readme_bodies.py
```

For figures:
```sh
uv run scripts/draw_figures.py
```

Both commands use the `files` list in `scripts/plot_config.py` to select benchmark results.

## Repository Structure

### Root Directory
- **go.sh**: Sets up and runs the benchmark pipeline.

### `benchmark/`
Per-implementation code and `setup.sh`, `build.sh`, and `run.sh` scripts.

### `figures/`
Contains PNG files visualizing benchmark results, such as:
- Initialization times
- Memory usage
- Latency and throughput metrics

### `logs/`
Benchmark logs, including full-run JSONL events in `full_runs/`.

### `results/`
Parsed benchmark results used by plots and tables.

### `scripts/`
Utility scripts for managing and visualizing benchmarks:
- **run.py**: Wraps `go.sh` or parses full-run JSONL into case lists or per-test CSV.
- **setup.sh**: Fetches and builds benchmark implementations.
- **reset.sh**: Removes benchmark build directories.
- **run_ramp_latency.sh**: Runs latency workloads.
- **parse.py**: Extracts benchmark reports from logs into JSON results.
- **draw_figures.py**: Generates benchmark figures.
- **generate_readme_bodies.py**: Generates Markdown benchmark tables.
- **plot_config.py**: Selects result files for plots and tables.
- **utils.py**: Shared result-loading, plotting and table helpers.
- **config_ubuntu.sh** / **config_arch.sh**: Install system dependencies.


## Contributing

Feel free to contribute to this repository by adding new benchmarks, improving scripts, or updating documentation. Take a look at [CONTRIBUTING.md](./CONTRIBUTING.md) for details on how to contribute.
