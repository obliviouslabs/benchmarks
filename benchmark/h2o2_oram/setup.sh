#!/bin/bash
set -e 

proj_name="h2o2_oram"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

cd "${base_dir}/build"

if [ -d "${proj_name}" ]; then
  rm -rf "./${proj_name}"
fi

# Clone the repo
git clone 'git@github.com:55199789/H2O2RAM.git' "$proj_name"

# Copy the benchmark code
cp -r "${sources_folder}/benchmark_code/"* "${build_folder}/benchmarks/"
cp "${base_dir}/benchmark/common/ramp_latency.h" "${build_folder}/benchmarks/"
mkdir -p "${build_folder}/bin/"
cp -r "${sources_folder}/cached_optimizations/"* "${build_folder}/bin/"

# The upstream benchmark target only glob-matches .cpp files. Keep the ramp
# adapter as .cc so it can provide its own main function in a separate binary.
printf '\ninclude(benchmarks/h2o2_oram_ramp.cmake)\n' >> "${build_folder}/CMakeLists.txt"
