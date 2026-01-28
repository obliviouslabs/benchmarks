#!/bin/bash
set -e 

proj_name="olabs_rostl"
repo_path="/rostl/"
base_dir=$(git rev-parse --show-toplevel)
source "${base_dir}/scripts/gen_args.sh"

cd "${base_dir}/build"
if [ -d "${project_name}" ]; then
  rm -rf "./${project_name}"
fi
mkdir -p "${build_folder}"

git clone 'git@github.com:obliviouslabs/rostl.git' "${build_folder}/${repo_path}"
# cp -r ../../rostl "${build_folder}/${repo_path}"

echo "Copying the benchmark code"
cp -r "${sources_folder}/benchmark_code/." "${build_folder}"
cp "${base_dir}/benchmark/common/common.rs" "${build_folder}/src"
