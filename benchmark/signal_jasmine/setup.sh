#!/bin/bash
set -e

proj_name="signal_jasmine"
repo_path="source"
signal_ref="${SIGNAL_JASMINE_REF:-origin/main}"
default_depth="${SIGNAL_JASMINE_PATH_LENGTH:-16}"
depths="${SIGNAL_JASMINE_PATH_LENGTHS:-10 11 12 13 14 15 16 17 18 19 20 21 22 23 24}"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

validate_depth()
{
  case "$1" in
    8|9|[12][0-9]|30) ;;
    *) echo "Jasmine path lengths must be integers from 8 to 30 (no leading zeros): $1" >&2; exit 1 ;;
  esac
}

# Save the requested sweep, without duplicates. Also prepare the fixed/ramp depth.
run_depths=""
for depth in $depths; do
  validate_depth "$depth"
  case " $run_depths " in
    *" $depth "*) ;;
    *) run_depths="${run_depths:+$run_depths }$depth" ;;
  esac
done
if [ -z "$run_depths" ]; then
  echo "SIGNAL_JASMINE_PATH_LENGTHS must contain at least one depth" >&2
  exit 1
fi
validate_depth "$default_depth"
build_depths="$run_depths"
case " $build_depths " in
  *" $default_depth "*) ;;
  *) build_depths="$build_depths $default_depth" ;;
esac

# Fetch one source revision and its submodules for all depths.
rm -rf "${build_folder%/}"
mkdir -p "$build_folder"
source_folder="${build_folder}/source"
git clone 'https://github.com/signalapp/ContactDiscoveryService-Icelake' "$source_folder"
git -C "$source_folder" checkout "$signal_ref"
git -C "$source_folder" submodule update --init --recursive

echo "Copying the benchmark code"
cp -r "${sources_folder}/benchmark_code/." "${source_folder}/c/"
cp "${base_dir}/benchmark/common/common.h" "${source_folder}/c/benchmarks/tests/"
cp "${base_dir}/benchmark/common/ramp_latency.h" "${source_folder}/c/benchmarks/tests/"

echo "Patching the shared build scripts"
sed -i 's/^##TESTS$/##TESTS\nTESTS=benchmarks\/path_oram.test benchmarks\/loaded_sharded_table.test benchmarks\/loaded_table.test benchmarks\/loaded_table_ramp.test/' "${source_folder}/c/Makefile"
sed -i 's/^tests: $(patsubst %,%.out,$(TESTS)) enclave.test.out constant_time_check.test$/tests: $(patsubst %,%.out,$(TESTS))/' "${source_folder}/c/Makefile"
sed -i '/^[[:space:]]*-g[[:space:]]*\\[[:space:]]*$/d' "${source_folder}/c/Makefile.base"
printf 'testsbin: $(TESTS)\n\techo ok\n\n' >> "${source_folder}/c/Makefile"

for signal_path_length in $build_depths; do
  depth_folder="${build_folder}/L${signal_path_length}"
  mkdir -p "$depth_folder"
  # Independent trees (including submodule metadata); use copy-on-write if available.
  cp -a --reflink=auto "${source_folder}/." "$depth_folder/"

  main_positions=$((1 << (signal_path_length - 1)))
  main_blocks=$((main_positions * 13 / 8))
  block_data_qwords=$(((((4096 / 3) / 8) * 8 - 16) / 8))
  position_map_entries_per_block=$((block_data_qwords * 2))
  position_map_blocks=$(((main_blocks + position_map_entries_per_block - 1) / position_map_entries_per_block))
  position_map_path_length=1
  position_map_positions=1
  while [ "$position_map_positions" -lt "$position_map_blocks" ]; do
    position_map_positions=$((position_map_positions * 2))
    position_map_path_length=$((position_map_path_length + 1))
  done

  echo "Preparing Jasmine L${signal_path_length} with 16 shards"
  sed -i -E 's/param int NUM_SHARDS = [0-9]+;/param int NUM_SHARDS = 16;/' "${depth_folder}/c/jasmin.test/params.jinc"
  sed -i -E "s/param int PATH_LENGTH = [0-9]+;/param int PATH_LENGTH = ${signal_path_length};/" "${depth_folder}/c/jasmin.test/params.jinc"
  sed -i -E "s/param int PATH_LENGTH_0 = [0-9]+;/param int PATH_LENGTH_0 = ${position_map_path_length};/" "${depth_folder}/c/jasmin.test/params.jinc"
  sed -i -E "s/param int POSITION_MAP_SIZE_0 = [0-9]+;/param int POSITION_MAP_SIZE_0 = ${position_map_blocks};/" "${depth_folder}/c/jasmin.test/params.jinc"

  sed -i -E "/oram_create_depth16\\(/,/return oram;/s/size_t num_levels = [0-9]+;/size_t num_levels = ${signal_path_length};/" "${depth_folder}/c/path_oram/path_oram.c"
  sed -i -E "/oram_create_depth16\\(/,/return oram;/s/size_t num_blocks = .*;/size_t num_blocks = ${main_blocks};/" "${depth_folder}/c/path_oram/path_oram.c"
  sed -i -E "/oram_create_depth16_posmap\\(/,/return oram;/s/size_t num_levels = [0-9]+;/size_t num_levels = ${position_map_path_length};/" "${depth_folder}/c/path_oram/path_oram.c"
  sed -i -E "/oram_create_depth16_posmap\\(/,/return oram;/s/size_t num_blocks = .*;/size_t num_blocks = ${position_map_blocks};/" "${depth_folder}/c/path_oram/path_oram.c"

  sed -i -E "/oram_position_map_create_depth16\\(/,/return result;/s/size_t num_block_ids_in_domain = [0-9]+;/size_t num_block_ids_in_domain = ${main_blocks};/" "${depth_folder}/c/path_oram/position_map.c"
  sed -i -E "/oram_position_map_create_depth16\\(/,/return result;/s/size_t num_positions_in_range = [0-9]+;/size_t num_positions_in_range = ${main_positions};/" "${depth_folder}/c/path_oram/position_map.c"
  sed -i -E "/scan_position_map_create_depth16\\(/,/return result;/s/size_t num_block_ids_in_domain = [0-9]+;/size_t num_block_ids_in_domain = ${position_map_blocks};/" "${depth_folder}/c/path_oram/position_map.c"
  sed -i -E "/scan_position_map_create_depth16\\(/,/return result;/s/size_t num_positions_in_range = [0-9]+;/size_t num_positions_in_range = ${position_map_positions};/" "${depth_folder}/c/path_oram/position_map.c"

  sed -i "s/^##TEST_CFLAGS$/  -DSIGNAL_JASMINE_PATH_LENGTH=${signal_path_length} \\\\\n##TEST_CFLAGS/" "${depth_folder}/c/Makefile.base"
done

printf '%s\n' "$run_depths" > "${build_folder}/depths"
# Keep the fixed-depth/ramp entry point, without another compilation.
ln -s "L${default_depth}/c" "${build_folder}/c"
