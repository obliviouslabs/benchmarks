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

# Extract assembly for manual inspection, feel free to comment these out:
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_64 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_32 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_16 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_8 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_4 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_2 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cmov_1 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_64 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_64 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_32 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_32 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_16 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_16 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_8 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_8 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_4 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_4 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_2 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_2 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cxchg_1 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib explicit_cswap_1 0
cargo asm --profile=maxperf -p olabs_rods_benchmark --lib heap_tree_read_path 0 

cargo run --profile=maxperf 2>&1 | stdbuf -oL tee "$base_dir/logs/$run_id/rods_bench.log"

# Parse the results
echo "" > "$results_file"
python "$base_dir/scripts/parse.py" -f "$base_dir/logs/$run_id/rods_bench.log" >> "$results_file"
