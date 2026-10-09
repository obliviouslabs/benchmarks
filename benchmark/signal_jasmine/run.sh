#!/bin/bash
set -e

proj_name="signal_jasmine"
repo_path="source"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"
run_label="${1:-}"
path_oram_args="${SIGNAL_JASMINE_PATH_ORAM_ARGS:-}"

if [ -n "${SIGNAL_JASMINE_PATH_LENGTHS:-}" ]; then
  depths="$SIGNAL_JASMINE_PATH_LENGTHS"
elif [ -f "${build_folder}/depths" ]; then
  IFS= read -r depths < "${build_folder}/depths"
else
  echo "Run benchmark/signal_jasmine/setup.sh and build.sh before running" >&2
  exit 1
fi

# Check the entire requested sweep before starting any benchmark. Never rebuild here.
set -- $depths
if [ "$#" -eq 0 ]; then
  echo "SIGNAL_JASMINE_PATH_LENGTHS must contain at least one depth" >&2
  exit 1
fi
for depth do
  case "$depth" in
    8|9|[12][0-9]|30) ;;
    *) echo "Invalid Jasmine path length: $depth (expected 8 to 30, no leading zeros)" >&2; exit 1 ;;
  esac
  for test_name in path_oram loaded_table loaded_sharded_table; do
    binary="${build_folder}/L${depth}/c/benchmarks/${test_name}.test"
    if [ ! -x "$binary" ]; then
      echo "Missing $binary; prepare and build this depth before running" >&2
      exit 1
    fi
  done
done

mkdir -p "${base_dir}/results" "$logs_folder"
: > "$results_file"
echo "Writing aggregate results to ${results_file}"
echo "Writing per-depth logs to ${logs_folder}"

run_test()
{
  test_name=$1
  shift
  log_file="${logs_folder}/${test_name}_L${depth}.out"
  "./${test_name}.test" "$@" 2>&1 | stdbuf -oL tee "$log_file"
  python "$base_dir/scripts/parse.py" -f "$log_file" >> "$results_file"
}

for depth do
  echo "Running Signal Jasmine benchmarks with PATH_LENGTH=${depth}"
  export BENCHMARK_VARIANT="${run_label:+${run_label}_}L${depth}"
  cd "${build_folder}/L${depth}/c/benchmarks"
  run_test path_oram $path_oram_args
  run_test loaded_table
  run_test loaded_sharded_table
done

echo "Done. Results: ${results_file}"
echo "Logs: ${logs_folder}"
