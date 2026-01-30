#!/bin/bash
set -e 

proj_name="olabs_rostl"
repo_path="/rostl/"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"


cd "${build_folder}"

cargo build --profile=maxperf

# Extract assembly for manual inspection, feel free to comment these out:
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_64 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_32 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_16 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_8 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_4 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_2 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cmov_1 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_64 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_64 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_32 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_32 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_16 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_16 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_8 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_8 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_4 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_4 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_2 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_2 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cxchg_1 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib explicit_cswap_1 0
cargo asm --profile=maxperf -p olabs_rostl_benchmark --lib heap_tree_read_path 0 

