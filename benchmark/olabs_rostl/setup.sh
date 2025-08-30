#!/bin/bash
set -e 

project_name="olabs_rostl"
base_dir=$(git rev-parse --show-toplevel)
echo "$base_dir"

cd "$base_dir/build"

if [ -d "${project_name}" ]; then
  rm -rf "./${project_name}"
fi
mkdir -p "$base_dir/build/${project_name}"


echo "Cloning the Rust Oblivious Standard Library repository"
git clone 'git@github.com:obliviouslabs/rostl.git' "$base_dir/build/${project_name}/rostl"
# cp -r ../../rostl "$base_dir/build/${project_name}/rostl"

echo "Copying the benchmark code"
cp -r "$base_dir/benchmark/${project_name}/benchmark_code/." "$base_dir/build/${project_name}"
cp "$base_dir/benchmark/common/common.rs" "$base_dir/build/${project_name}/src"
