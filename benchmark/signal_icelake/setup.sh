#!/bin/bash
set -e 

base_dir=$(git rev-parse --show-toplevel)
echo $base_dir

cd $base_dir/build

if [ -d "ContactDiscoveryService-Icelake" ]; then
  rm -rf ContactDiscoveryService-Icelake
fi

echo "Cloning the Icelake branch of the ContactDiscoveryService"
git clone 'https://github.com/signalapp/ContactDiscoveryService-Icelake'
cd ContactDiscoveryService-Icelake 
git submodule init
git submodule update --init --recursive
cd ..

echo "Copying the benchmark code"
cp -r $base_dir'/benchmark/signal_icelake/benchmark_code/.' $base_dir/build/ContactDiscoveryService-Icelake/c/
cp $base_dir'/benchmark/common/common.h' $base_dir/build/ContactDiscoveryService-Icelake/c/benchmarks/tests

echo "Patching the build script"
sed -i 's/^##TESTS$/##TESTS\nTESTS=benchmarks\/path_oram.test/' $base_dir/build/ContactDiscoveryService-Icelake/c/Makefile
# sed -i 's/^##TESTS$/##TESTS\nTESTS=benchmarks\/path_oram.test benchmarks\/loaded_sharded_table.test benchmarks\/loaded_table.test/' $base_dir/build/ContactDiscoveryService-Icelake/c/Makefile
sed -i 's/^tests: $(patsubst %,%.out,$(TESTS)) enclave.test.out constant_time_check.test$/tests: $(patsubst %,%.out,$(TESTS))/' $base_dir/build/ContactDiscoveryService-Icelake/c/Makefile

# sed -i 's/^##TESTS$/##TESTS\nTESTS=benchmarks\/loaded_sharded_table.test/' $base_dir/build/ContactDiscoveryService-Icelake/c/Makefile
sed -i '/^[[:space:]]*-g[[:space:]]*\\[[:space:]]*$/d' $base_dir/build/ContactDiscoveryService-Icelake/c/Makefile.base

