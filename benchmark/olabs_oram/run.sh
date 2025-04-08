#!/bin/bash
set -e 

base_dir=$(git rev-parse --show-toplevel)
commit_hash=$(git -C $base_dir/build/olabs_oram/ rev-parse HEAD)
timestamp=$(date +%s)
run_id=olabs_oram_${timestamp}_${commit_hash}
results_file=$base_dir/results/${run_id}
run_folder=$base_dir/build/olabs_oram/

# Create the logs folder
mkdir -p "$base_dir/logs/$run_id"

# Run the tests
cd "$run_folder"
rm -rf build
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/applications/benchmarks/oram_bench 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/oram_bench.log"
./build/applications/benchmarks/umap 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/umap.log"
./build/applications/benchmarks/umap_sharded 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/umap_sharded.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/oram_bench.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/umap.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/umap_sharded.log" >> "$results_file"
