#!/bin/bash
set -e 

proj_name="olabs_oram"
base_dir=$(git rev-parse --show-toplevel)
. " ${base_dir}/scripts/gen_args.sh"

cd "${base_dir}/build"
if [ -d "${proj_name}" ]; then
  rm -rf "${proj_name}"
fi

git clone 'git@github.com:obliviouslabs/oram.git' "${proj_name}"

echo "Copying the benchmark code"
cp -r "${sources_folder}/benchmark_code/." "${build_folder}/applications/"
cp "${base_dir}/benchmark/common/common.h" "${build_folder}/applications/benchmarks/benchmark"
cp "${base_dir}/benchmark/common/cds.proto" "${build_folder}/applications/benchmarks/benchmark"

echo "Patching the build script"
sed -i '/add_subdirectory(tests)/a add_subdirectory(applications/benchmarks)' "${build_folder}/CMakeLists.txt"

