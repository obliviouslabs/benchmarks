#!/bin/bash
set -e 

proj_name="mc_oblivious"
repo_path="/mc-oblivious/"
base_dir=$(git rev-parse --show-toplevel)
source "${base_dir}/scripts/gen_args.sh"


cd "${build_folder}"
cargo build --profile=maxperf