#!/bin/bash
set -e 

project_name="h2o2_oram"
base_dir=$(git rev-parse --show-toplevel)
echo $base_dir

cd $base_dir/build

if [ -d "$project_name" ]; then
  rm -rf $project_name
fi

echo "Cloning the H2O2 ORAM c++ repository"
git clone 'git@github.com:55199789/H2O2RAM.git' h2o2_oram


cp -r "$base_dir/benchmark/${project_name}/benchmark_code/"* "$base_dir/build/${project_name}/benchmarks/"
mkdir -p "$base_dir/build/${project_name}/bin/"
cp -r "$base_dir/benchmark/${project_name}/cached_optimizations/"* "$base_dir/build/${project_name}/bin/"

echo "Copying the benchmark code"

echo "Patching the build script"

