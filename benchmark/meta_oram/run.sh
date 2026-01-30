#!/bin/bash
set -e 

proj_name="meta_oram"
repo_path="/meta/"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

# Create the logs folder
mkdir -p "${logs_folder}"

# Run the tests
cd "$run_folder"
echo "Running benchmark for $proj_name..."
cargo build --profile=maxperf
cargo run --profile=maxperf 2>&1 | tee "${logs_folder}/meta_bench.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/meta_bench.log" >> "$results_file"
