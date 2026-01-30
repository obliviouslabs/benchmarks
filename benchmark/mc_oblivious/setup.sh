#!/bin/bash
set -e 

proj_name="mc_oblivious"
repo_path="/mc-oblivious/"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

cd "$base_dir/build"

if [ -d "${proj_name}" ]; then
  rm -rf "./${proj_name}"
fi

mkdir -p "${build_folder}"

# Clone the repo
git clone 'git@github.com:mobilecoinfoundation/mc-oblivious.git' "${build_folder}/mc-oblivious"

# Copy the benchmark code
cp -r "${sources_folder}/benchmark_code/." "${build_folder}"
cp "$base_dir/benchmark/common/common.rs" "${build_folder}/src"
