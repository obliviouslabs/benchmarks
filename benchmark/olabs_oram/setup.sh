#!/bin/bash
set -e 

base_dir=$(git rev-parse --show-toplevel)
echo $base_dir

cd $base_dir/build

if [ -d "olabs_oram" ]; then
  rm -rf olabs_oram
fi

echo "Cloning the Oblivious Labs ORAM c++ repository"
git clone 'git@github.com:obliviouslabs/oram.git' olabs_oram

echo "Copying the benchmark code"
cp -r $base_dir'/benchmark/olabs_oram/benchmark_code/.' $base_dir/build/olabs_oram/applications/
cp $base_dir'/benchmark/common/common.h' $base_dir/build/olabs_oram/applications/benchmarks/benchmark

echo "Patching the build script"
sed -i '/add_subdirectory(tests)/a add_subdirectory(applications/benchmarks)'  $base_dir/build/olabs_oram/CMakeLists.txt

