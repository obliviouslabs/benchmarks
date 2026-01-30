#!/bin/bash
set -e 

proj_name="olabs_oram"
base_dir=$(git rev-parse --show-toplevel)
. " ${base_dir}/scripts/gen_args.sh"

# Create the logs folder
mkdir -p "${logs_folder}"

# Run the tests
cd "${build_folder}"
./build/applications/benchmarks/oram_bench 2>&1 | stdbuf -oL tee "${logs_folder}/oram_bench.log"
./build/applications/benchmarks/umap 2>&1 | stdbuf -oL tee "${logs_folder}/umap.log"
./build/applications/benchmarks/umap_sharded 2>&1 | stdbuf -oL tee "${logs_folder}/umap_sharded.log"
./build/applications/benchmarks/load_balancing 2>&1 | stdbuf -oL tee "${logs_folder}/load_balancing.log"
./build/applications/benchmarks/rw_signal 2>&1 | stdbuf -oL tee "${logs_folder}/rw_signal.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/oram_bench.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/umap.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/umap_sharded.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/load_balancing.log" >> "$results_file"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/rw_signal.log" >> "$results_file"
