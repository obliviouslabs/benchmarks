#!/bin/bash
set -e 

proj_name="meta_oram"
repo_path="/meta/"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

cd "$base_dir/build"
if [ -d "${proj_name}" ]; then
  rm -rf "./${proj_name}"
fi
mkdir -p "${build_folder}"

git clone 'git@github.com:facebook/oram.git' "${build_folder}/meta"

echo "Copying the benchmark code"
cp -r "${sources_folder}/benchmark_code/." "${build_folder}"
cp "$base_dir/benchmark/common/common.rs" "${build_folder}/src"