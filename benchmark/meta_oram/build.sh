#!/bin/bash
set -e 

proj_name="meta_oram"
repo_path="/meta/"
base_dir=$(git rev-parse --show-toplevel)
source "${base_dir}/scripts/gen_args.sh"

cd "${build_folder}"
cargo build --profile=maxperf