#!/bin/bash
set -e 

proj_name="signal_jasmine"
signal_ref="${SIGNAL_JASMINE_REF:-origin/main}"
signal_path_length="${SIGNAL_JASMINE_PATH_LENGTH:-16}"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

case "${signal_path_length}" in
  ''|*[!0-9]*)
    echo "SIGNAL_JASMINE_PATH_LENGTH must be an integer" >&2
    exit 1
    ;;
esac

if [ "${signal_path_length}" -lt 8 ]; then
  echo "SIGNAL_JASMINE_PATH_LENGTH must be at least 8" >&2
  exit 1
fi

if [ "${signal_path_length}" -gt 30 ]; then
  echo "SIGNAL_JASMINE_PATH_LENGTH must be at most 30" >&2
  exit 1
fi

main_positions=$((1 << (signal_path_length - 1)))
main_blocks=$((main_positions * 13 / 8))
block_data_qwords=$(((((4096 / 3) / 8) * 8 - 16) / 8))
position_map_entries_per_block=$((block_data_qwords * 2))
position_map_blocks=$(((main_blocks + position_map_entries_per_block - 1) / position_map_entries_per_block))
position_map_path_length=1
position_map_positions=1
while [ "${position_map_positions}" -lt "${position_map_blocks}" ]; do
  position_map_positions=$((position_map_positions * 2))
  position_map_path_length=$((position_map_path_length + 1))
done

cd $base_dir/build
if [ -d "${proj_name}" ]; then
  rm -rf "${proj_name}"
fi

git clone 'https://github.com/signalapp/ContactDiscoveryService-Icelake' "${proj_name}"
cd "${proj_name}"
git checkout "${signal_ref}"
git submodule update --init --recursive
cd ..

echo "Using 16 shards for the Jasmine benchmark test build"
sed -i -E 's/param int NUM_SHARDS = [0-9]+;/param int NUM_SHARDS = 16;/' "${build_folder}/c/jasmin.test/params.jinc"

echo "Using path length ${signal_path_length} for the Jasmine benchmark test ORAM"
sed -i -E "s/param int PATH_LENGTH = [0-9]+;/param int PATH_LENGTH = ${signal_path_length};/" "${build_folder}/c/jasmin.test/params.jinc"
sed -i -E "s/param int PATH_LENGTH_0 = [0-9]+;/param int PATH_LENGTH_0 = ${position_map_path_length};/" "${build_folder}/c/jasmin.test/params.jinc"
sed -i -E "s/param int POSITION_MAP_SIZE_0 = [0-9]+;/param int POSITION_MAP_SIZE_0 = ${position_map_blocks};/" "${build_folder}/c/jasmin.test/params.jinc"

sed -i -E "/oram_create_depth16\\(/,/return oram;/s/size_t num_levels = [0-9]+;/size_t num_levels = ${signal_path_length};/" "${build_folder}/c/path_oram/path_oram.c"
sed -i -E "/oram_create_depth16\\(/,/return oram;/s/size_t num_blocks = .*;/size_t num_blocks = ${main_blocks};/" "${build_folder}/c/path_oram/path_oram.c"
sed -i -E "/oram_create_depth16_posmap\\(/,/return oram;/s/size_t num_levels = [0-9]+;/size_t num_levels = ${position_map_path_length};/" "${build_folder}/c/path_oram/path_oram.c"
sed -i -E "/oram_create_depth16_posmap\\(/,/return oram;/s/size_t num_blocks = .*;/size_t num_blocks = ${position_map_blocks};/" "${build_folder}/c/path_oram/path_oram.c"

sed -i -E "/oram_position_map_create_depth16\\(/,/return result;/s/size_t num_block_ids_in_domain = [0-9]+;/size_t num_block_ids_in_domain = ${main_blocks};/" "${build_folder}/c/path_oram/position_map.c"
sed -i -E "/oram_position_map_create_depth16\\(/,/return result;/s/size_t num_positions_in_range = [0-9]+;/size_t num_positions_in_range = ${main_positions};/" "${build_folder}/c/path_oram/position_map.c"
sed -i -E "/scan_position_map_create_depth16\\(/,/return result;/s/size_t num_block_ids_in_domain = [0-9]+;/size_t num_block_ids_in_domain = ${position_map_blocks};/" "${build_folder}/c/path_oram/position_map.c"
sed -i -E "/scan_position_map_create_depth16\\(/,/return result;/s/size_t num_positions_in_range = [0-9]+;/size_t num_positions_in_range = ${position_map_positions};/" "${build_folder}/c/path_oram/position_map.c"

echo "Copying the benchmark code"
cp -r "${sources_folder}/benchmark_code/." "${build_folder}/c/"
cp $base_dir'/benchmark/common/common.h' "${build_folder}/c/benchmarks/tests"
cp $base_dir'/benchmark/common/ramp_latency.h' "${build_folder}/c/benchmarks/tests"

echo "Patching the build script"
sed -i 's/^##TESTS$/##TESTS\nTESTS=benchmarks\/path_oram.test benchmarks\/loaded_sharded_table.test benchmarks\/loaded_table.test benchmarks\/loaded_table_ramp.test/' "${build_folder}/c/Makefile"
sed -i 's/^tests: $(patsubst %,%.out,$(TESTS)) enclave.test.out constant_time_check.test$/tests: $(patsubst %,%.out,$(TESTS))/' "${build_folder}/c/Makefile"

sed -i '/^[[:space:]]*-g[[:space:]]*\\[[:space:]]*$/d' "${build_folder}/c/Makefile.base"
sed -i "s/^##TEST_CFLAGS$/  -DSIGNAL_JASMINE_PATH_LENGTH=${signal_path_length} \\\\\n##TEST_CFLAGS/" "${build_folder}/c/Makefile.base"

printf 'testsbin: $(TESTS)\n\techo ok\n\n' >> "${build_folder}/c/Makefile"
