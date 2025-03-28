#!/bin/bash
set -e 

project_name="meta_oram"
base_dir=$(git rev-parse --show-toplevel)
echo "$base_dir"
mkdir -p "$base_dir/build"
cd "$base_dir/build"

if [ -d "${project_name}" ]; then
  rm -rf "./${project_name}"
fi
mkdir -p "$base_dir/build/${project_name}"


git clone 'git@github.com:facebook/oram.git' "$base_dir/build/${project_name}/meta"
#cp -r "$base_dir/../rods/." "$base_dir/build/${project_name}/rods"

echo "Copying the benchmark code"
cp -r "$base_dir/benchmark/${project_name}/benchmark_code/." "$base_dir/build/${project_name}"
