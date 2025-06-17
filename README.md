# Benchmarks Repository

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

### Recursive ORAM (RORAM)

### Unordered Map (UMAP)

#### Single Threaded

#### Multi-threaded with 15 partitions


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
