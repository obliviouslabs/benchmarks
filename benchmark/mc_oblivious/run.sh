#!/bin/bash
set -e 

proj_name="mc_oblivious"
repo_path="/mc-oblivious/"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

# Create the logs folder
mkdir -p "${logs_folder}"
cd "${build_folder}"

# Run the tests
cargo run --profile=maxperf 2>&1 | stdbuf -oL tee "${logs_folder}/${proj_name}_bench.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/${proj_name}_bench.log" >> "$results_file"
