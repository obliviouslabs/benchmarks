#!/bin/bash
set -e 

proj_name="signal_icelake"
base_dir=$(git rev-parse --show-toplevel)
. " ${base_dir}/scripts/gen_args.sh"

mkdir -p "${logs_folder}"
echo "" > "$results_file"

cd "${build_folder}/c/benchmarks/"

# ./path_oram.test 2>&1 | stdbuf -oL tee "${logs_folder}/path_oram.out"
# python "$base_dir/scripts/parse.py" -f "${logs_folder}/path_oram.out" >> "$results_file"

# ./loaded_table.test 2>&1 | stdbuf -oL tee "${logs_folder}/loaded_table.out"
# python "$base_dir/scripts/parse.py" -f "${logs_folder}/loaded_table.out" >> "$results_file"

# ./loaded_sharded_table.test 2>&1 | stdbuf -oL tee "${logs_folder}/loaded_sharded_table.out"
# python "$base_dir/scripts/parse.py" -f "${logs_folder}/loaded_sharded_table.out" >> "$results_file"

./loaded_sharded_table.test best 2>&1 | stdbuf -oL tee "${logs_folder}/loaded_sharded_table.out"
python "$base_dir/scripts/parse.py" -f "${logs_folder}/loaded_sharded_table.out" >> "$results_file"

