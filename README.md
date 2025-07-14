# Oblivious Algorithms Benchmarks

This repository contains reproducible benchmarks related to several implementations of ORAM, Oblivious Maps and other oblivious algorithms targeting TEEs (Trusted Execution Environments). To reproduce, simply run `go.sh`. Our goal with this repo is to provide transparent analysis of the performance of several oblivious algorithms, and to incentivize the development of efficient implementations of oblivious algorithms.

See [benchmark types](#benchmark-types) for information about the algorithms that are being benchmarked and implementations for each algorithm and [benchmark results](#benchmark-results) for a summary of the results of each implementation.
Contributions with new implementations are welcome, see [CONTRIBUTING.md](./CONTRIBUTING.md) for information on how to add new benchmarks.


## Benchmark Types

| Name | Description | Measured Parameters | Independent Variables |
| :-   | :-          | :-:                 | :- |
| NRORAM | Non recursive ORAM (Requires storing the position somewhere for accesses) | Read Latency, Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Address size), V (Value size) |
| RORAM | Recursive ORAM - Basically an array | Read Latency, Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Address size), V (Value size)
| UMAP | Unordered Map / Key-Value Store (Many disperse keys) | Read Latency, Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Key size), V (Value size)
| Sharded-UMAP | Sharded UMap | Batch Read Latency, Batch Read Throughput, Initialization Time, Memory Usage | N (ORAM size), K (Key size), V (Value size), P (Number of Partitions), B (Queries per batch)

## Benchmarked Implementations

| Name | Description | Language | Types |
| :-   | :-          | :-       | :-:   |
| [**olabs_oram**](https://github.com/obliviouslabs/oram) | Oblivious Labs's SGX implementation of ORAM and UMAP | C++ | NRORAM, RORAM, UMAP, Sharded-UMAP |
| [**signal_icelake**](https://github.com/signalapp/ContactDiscoveryService-Icelake) | Signal's implemenation of ORAM and UMAP for Private Contact Discovery | C++ | UMAP, Sharded-UMAP |
| [**olabs_rostl**](https://github.com/obliviouslabs/rostl) | Oblivious Labs's Rust Oblivious Standard Library implementation of ORAM and UMAP | Rust |  NRORAM, RORAM, UMAP, Sharded-UMAP |
| [**mc_oblivious**](https://github.com/mobilecoinfoundation/mc-oblivious) | Mobilecoin's implementation of ORAM and UMAP | Rust | RORAM, UMAP |
| [**meta_oram**](https://github.com/facebook/oram) | Meta's implementation of RORAM | Rust | RORAM |

## Benchmark Results

### Environment

All the benchmarks are ran on the same environment. A mini server with an AMD Ryzen 9 7945HX with 64GB of RAM, and an WD Black SN850 SSD, running an up to date version of arch linux.

### Non-Recursive ORAM (NRORAM)

# NRORAM - Latency

Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values 

| N | olabs_rostl | olabs_oram |
| :--- |  :---: | :---: |
| $2^{10}$ | **0.476434077** | 0.55 |
| $2^{11}$ | **0.502171127** | 0.6 |
| $2^{12}$ | **0.53308976** | 0.67 |
| $2^{13}$ | **0.566434882** | 0.69 |
| $2^{14}$ | **0.603820885** | 0.74 |
| $2^{15}$ | **0.6531125289999999** | 0.77 |
| $2^{16}$ | **0.6924294790000001** | 0.82 |
| $2^{17}$ | **0.7384146580000001** | 0.83 |
| $2^{18}$ | **0.7838900169999999** | 0.87 |
| $2^{19}$ | **0.8851301509999999** | 0.91 |
| $2^{20}$ | **0.985728089** | 1.02 |
| $2^{21}$ | **1.0256044649999998** | 1.03 |
| $2^{22}$ | **1.084596443** | 1.08 |
| $2^{23}$ | 1.1460931909999998 | **1.08** |
| $2^{24}$ | 1.312990616 | **1.16** |
| $2^{25}$ | 1.516923724 | **1.31** |
| $2^{26}$ | 1.733630409 | **1.44** |
| $2^{27}$ | 1.964124429 | **1.55** |
| $2^{28}$ | 2.2392847820000004 | **1.78** |

### Recursive ORAM (RORAM)

# RORAM - Latency

Latency (us) vs ORAM number of entries (N), using 8 byte keys, 8 byte values 

| N | meta_oram | olabs_oram | olabs_rostl |
| :--- |  :---: | :---: | :---: |
| $2^{10}$ | 31.99438977 | **0.32** | 0.544331452 |
| $2^{11}$ | 36.33813978 | **0.38** | 0.63029241 |
| $2^{12}$ | 42.4064812 | **0.91** | 1.30999802 |
| $2^{13}$ | 51.341687310000005 | **0.96** | 1.329136356 |
| $2^{14}$ | 66.20758206000001 | **1.13** | 1.3912766540000001 |
| $2^{15}$ | 91.62640303 | 1.81 | **1.399630466** |
| $2^{16}$ | 139.79759206 | **2.01** | 2.368478346 |
| $2^{17}$ | 229.0843319 | 2.4 | **2.3602808239999997** |
| $2^{18}$ | 149.55286017999998 | 3.18 | **2.516789814** |
| $2^{19}$ | 201.49290409 | 3.7 | **2.561486344** |
| $2^{20}$ | 296.34107629 | 4.21 | **4.202789802** |
| $2^{21}$ | 219.93572774999998 | 5.22 | **4.2463743439999995** |
| $2^{22}$ | 276.29379165 | 5.85 | **4.318071794000001** |
| $2^{23}$ | 376.70006951 | 6.79 | **4.312663467999999** |
| $2^{24}$ | 304.78874277999995 | 7.94 | **6.622949732** |
| $2^{25}$ | 367.06124783 | 8.68 | **6.742583438** |
| $2^{26}$ | 470.23260369 | 9.59 | **7.007694596** |
| $2^{27}$ |  | **11.05** |  |
| $2^{28}$ |  | **12.26** |  |

### Unordered Map (UMAP)

#### Single Threaded

Latency (us) vs Map number of entries (N), using 8 byte keys, 56 byte values 

| N | Signal | olabs_rostl | olabs_umap_shortkv | olabs_umap |
| :--- |  :---: | :---: | :---: | :---: |
| $2^{10}$ | 7.65 | 1.95286744 | 3.44 | **1.51** |
| $2^{11}$ | 8.81 | 3.425139218 | 2.18 | **2.11** |
| $2^{12}$ | 10.57 | 3.507636062 | **2.36** | 2.53 |
| $2^{13}$ | 12.96 | 3.743701278 | 2.76 | **2.62** |
| $2^{14}$ | 14.75 | 4.350131888 | 3.29 | **3.04** |
| $2^{15}$ | 16.61 | 6.877253120000001 | 3.68 | **3.61** |
| $2^{16}$ | 19.36 | 7.301164874 | **4.07** | 4.11 |
| $2^{17}$ | 24.37 | 7.6929276479999995 | 4.57 | **4.39** |
| $2^{18}$ | 33.81 | 8.091950414 | 5.53 | **5.02** |
| $2^{19}$ | 33.96 | 12.121400586 | 6.17 | **5.9** |
| $2^{20}$ | 38.28 | 12.731309012 | 6.94 | **6.77** |
| $2^{21}$ | 43.11 | 13.329014687999999 | 7.59 | **7.38** |
| $2^{22}$ | 48.77 | 14.094304768 | 8.62 | **8.13** |
| $2^{23}$ | 56.38 | 18.90335933 | 9.53 | **9.03** |
| $2^{24}$ | 65.83 |  | 10.3 | **10.04** |
| $2^{25}$ | 81.48 |  | 11.29 | **11.07** |
| $2^{26}$ |  |  | **12.3** |  |

#### Multi-threaded with 15 partitions

###### OMAP - Sharded Throughput
 Througput (queries per second) vs Map number of entries (N), using 8 byte keys, 56 byte values, 15 Shards 

| N | olabs_umap_sharded-4096 | olabs_rostl_sharded-4096 | Signal_Sharded-4096 |
| :--- |  :---: | :---: | :---: |
| $2^{10}$ | 297896.72 | **1471156.3863867463** |  |
| $2^{11}$ | 629971.25 |  |  |
| $2^{12}$ | **1234160.95** | 983129.102154232 | 198452.91 |
| $2^{13}$ | **1373069.9** | 1277664.9271935336 | 130044.45 |
| $2^{14}$ | **693914.76** |  | 300086.56 |
| $2^{15}$ | **709560.87** |  | 272384.73 |
| $2^{16}$ | **808834.46** |  | 251091.91 |
| $2^{17}$ | **987172.01** | 608924.9039199668 | 182582.01 |
| $2^{18}$ | **772447.25** | 495686.8357185696 | 154155.17 |
| $2^{19}$ | **750383.43** | 408893.6479059182 | 126428.27 |
| $2^{20}$ | **713745.44** | 367729.67119521 | 111731.96 |
| $2^{21}$ | **643148.98** | 325616.1774706444 | 93213.9 |
| $2^{22}$ | **595496.22** | 275033.41301597323 | 85082.81 |
| $2^{23}$ | **490083.88** | 128226.5321881506 | 63448.79 |
| $2^{24}$ | **463556.08** |  | 62168.17 |
| $2^{25}$ | **424407.51** |  | 48556.98 |
| $2^{26}$ | **395608.99** |  | 50216.89 |

## Reproducing Benchmarks

### Running Benchmarks
Use the `go.sh` script to initiate benchmarking processes:
```bash
./go.sh
```

### Generating Figures
To generate figures from benchmark data, use the `draw.sh` script:
```bash
./scripts/draw.sh
```


## Repository Structure

### Root Directory
- **go.sh**: A script to initiate benchmarking processes.

### `benchmark/`
Contains the code for each benchmarked implementation. Typically we sepparate each folder in `setup.sh` (setup all the benchmark environment in build/) and `run.sh` (cd into the build file an run the benchmark):

### `figures/`
Contains PNG files visualizing benchmark results, such as:
- Initialization times
- Memory usage
- Latency and throughput metrics

### `logs/`
Contains logs from benchmarking runs, organized by system and timestamp.

### `results/`
Contains processed results from benchmarking runs.

### `scripts/`
Contains utility scripts for managing and visualizing benchmarks:
- **draw_figure.py**: A Python script for generating figures from benchmark data.
- **draw.sh**: A shell script for automating figure generation, it generates all the images in `figures/` from the log outputs.
- **setup.sh**: A setup script for preparing the environment.
- **parse.py**: An auxiliary python script for parsing benchmark logs in `logs/` into the json format in `results/`. 


## Contributing

Feel free to contribute to this repository by adding new benchmarks, improving scripts, or updating documentation. Take a look at [CONTRIBUTING.md](./CONTRIBUTING.md) for details on how to contribute.
