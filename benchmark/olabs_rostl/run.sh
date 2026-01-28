#!/bin/bash
set -e 

proj_name="olabs_rostl"
repo_path="/rostl/"
base_dir=$(git rev-parse --show-toplevel)
source "${base_dir}/scripts/gen_args.sh"

# Create the logs folder
mkdir -p "${logs_folder}"

# Run the tests
cargo run --profile=maxperf 2>&1 | stdbuf -oL tee "${logs_folder}/rostl_bench.log"

# Parse the results
echo "" > "$results_file"
python "${base_dir}/scripts/parse.py" -f "${logs_folder}/rostl_bench.log" >> "$results_file"