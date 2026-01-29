#!/bin/bash
set -e 

proj_name="h2o2_oram"
base_dir=$(git rev-parse --show-toplevel)
source "${base_dir}/scripts/gen_args.sh"


# Create the logs folder
mkdir -p "${logs_folder}/results"

# Run the tests
cd "${build_folder}/benchmarks"
PYTHONUNBUFFERED=1 stdbuf -oL -eL python oram_exp_script.py "${logs_folder}" 2>&1 | stdbuf -oL tee "${logs_folder}/bench_oram.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/bench_oram.log" >> "$results_file"
