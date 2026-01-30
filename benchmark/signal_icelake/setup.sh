#!/bin/bash
set -e 

proj_name="signal_icelake"
base_dir=$(git rev-parse --show-toplevel)
. " ${base_dir}/scripts/gen_args.sh"

cd $base_dir/build
if [ -d "${proj_name}" ]; then
  rm -rf "${proj_name}"
fi

git clone 'https://github.com/signalapp/ContactDiscoveryService-Icelake' "${proj_name}"
cd ${proj_name}
git submodule init
git submodule update --init --recursive
cd ..

echo "Copying the benchmark code"
cp -r "${sources_folder}/benchmark_code/." "${build_folder}/c/"
cp $base_dir'/benchmark/common/common.h' "${build_folder}/c/benchmarks/tests"

echo "Patching the build script"
sed -i 's/^##TESTS$/##TESTS\nTESTS=benchmarks\/path_oram.test benchmarks\/loaded_sharded_table.test benchmarks\/loaded_table.test/' "${build_folder}/c/Makefile"
sed -i 's/^tests: $(patsubst %,%.out,$(TESTS)) enclave.test.out constant_time_check.test$/tests: $(patsubst %,%.out,$(TESTS))/' "${build_folder}/c/Makefile"

sed -i '/^[[:space:]]*-g[[:space:]]*\\[[:space:]]*$/d' "${build_folder}/c/Makefile.base"

printf 'testsbin: $(TESTS)\n\techo ok\n\n' >> "${build_folder}/c/Makefile"