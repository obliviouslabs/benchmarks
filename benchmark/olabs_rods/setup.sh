#!/bin/bash
set -e 

project_name="olabs_rods"
base_dir=$(git rev-parse --show-toplevel)
echo "$base_dir"

cd "$base_dir/build"

if [ -d "${project_name}" ]; then
  rm -rf "./${project_name}"
fi
mkdir -p "$base_dir/build/${project_name}"


# git clone 'git@github.com:xtrm0/rods.git' "$base_dir/build/${project_name}/rods"
cp -r "$base_dir/../rods/." "$base_dir/build/${project_name}/rods"

echo "Copying the benchmark code"
cp -r "$base_dir/benchmark/${project_name}/benchmark_code/." "$base_dir/build/${project_name}"
