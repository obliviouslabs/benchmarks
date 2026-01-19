#!/bin/bash
set -e 

base_dir=$(git rev-parse --show-toplevel)
commit_hash=$(git -C $base_dir/build/h2o2_oram/ rev-parse HEAD)
timestamp=$(date +%s)
run_id=h2o2_oram_${timestamp}_${commit_hash}
results_file=$base_dir/results/${run_id}
run_folder=$base_dir/build/h2o2_oram/

# Create the logs folder
mkdir -p "$base_dir/logs/$run_id/results"

# Run the tests
cd "$run_folder"
rm -rf build
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

cd benchmarks
stdbuf -oL -eL python oram_exp_script.py "$base_dir/logs/$run_id" 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/bench_oram.log"
stdbuf -oL -eL python omap_exp_script.py "$base_dir/logs/$run_id" 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/bench_omap.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/bench_oram.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/bench_omap.log" >> "$results_file"
