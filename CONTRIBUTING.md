# Adding a benchmark

To add a benchmark, please follow the following steps

0) Fork the repo.

1) Create a new folder inside of build with the name of the benchmarked test and the test content.

2) Add the `setup.sh` file that should clone the benchmarked test, and a `run.sh` file that should run the benchmarked test. Consider using the appropriate `common.h`/`common.rs` to help with test launching and reporting.

3) Add lines to `scripts/setup.sh` and `go.sh` to call your setup and run.

4) Test your benchmark, add a comment to your code documenting around how much time your test takes.

5) Edit the README.md to include your benchmark in the section Benchmarked Implementations.

6) Submit a PR, we will review it as soon as possible and rerun the benchmarks to update the repo. Please do not includde log/result files in the PR, we will rerun it in the same benchmark machine.

# Adding a benchmarked type/setting to an existing benchmark

0) Fork the repo

1) Modify the folder for the benchmark to add your new tested type / setting.

2) Modify the section Benchmarked Implementations (and maybe Benchmark Types) to include the type / setting.

3) Test your changes.

4) Submit a PR, we will review it as soon as possible and rerun the benchmarks to update the repo. Please do not includde log/result files in the PR, we will rerun it in the same benchmark machine.


# Report deviation

If you ran the benchmarks in some specific hardware and find deviations, please create a new issue with the following format.
Please make sure that you ran the benchmarks in an isolated environment and that you have enough RAM (64gb)

```
Tittle: Benchmark deviation

Description: The benchmark _______ has a signification deviation in a machine with _______ processor and ______ GB of RAM. The benchark figure shows ..., but we obtained ... .   

(include different benchmark results to support the claim)

```
