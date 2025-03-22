#!/bin/bash
set -e 

project_name="olabs_rods"
base_dir="$(git rev-parse --show-toplevel)"
commit_hash="$(git -C $base_dir/build/${project_name}/ rev-parse HEAD)"
timestamp="$(date +%s)"
run_id="${project_name}_${timestamp}_${commit_hash}"
results_file="$base_dir/results/${run_id}"
run_folder="$base_dir/build/${project_name}/"

# Create the logs folder
mkdir -p "$base_dir/logs/$run_id"

# Run the tests
cd "$run_folder"
cargo build --profile=maxperf
cargo run --profile=maxperf 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/rods_bench.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/rods_bench.log" >> "$results_file"
