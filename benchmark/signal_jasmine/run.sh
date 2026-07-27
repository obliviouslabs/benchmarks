#!/bin/bash
set -e

proj_name="signal_jasmine"
base_dir=$(git rev-parse --show-toplevel)
depths="${SIGNAL_JASMINE_PATH_LENGTHS:-10 12 14 16 18 20 21 22 23 24}"
run_timestamp=$(date +%s)
run_label="${1:-}"
BENCHMARK_TEST_TIMEOUT_MS="${BENCHMARK_TEST_TIMEOUT_MS:-14400000}"
export BENCHMARK_TEST_TIMEOUT_MS

results_file=""
logs_folder=""

for depth in $depths; do
  cd "$base_dir"
  echo "Running Signal Jasmine benchmarks with PATH_LENGTH=${depth}"
  sh "${base_dir}/scripts/reset.sh" signal_jasmine
  SIGNAL_JASMINE_PATH_LENGTH="${depth}" sh "${base_dir}/benchmark/signal_jasmine/setup.sh"
  sh "${base_dir}/benchmark/signal_jasmine/build.sh"

  if [ -z "$results_file" ]; then
    commit_hash="$(git -C "${base_dir}/build/${proj_name}" rev-parse HEAD)"
    run_id="${proj_name}_${run_timestamp}_${commit_hash}"
    if [ -n "$run_label" ]; then
      run_id="${run_id}_${run_label}"
    fi
    results_file="${base_dir}/results/${run_id}"
    logs_folder="${base_dir}/logs/${run_id}"
    mkdir -p "$logs_folder"
    : > "$results_file"
    echo "Writing aggregate results to ${results_file}"
    echo "Writing per-depth logs to ${logs_folder}"
  fi

  depth_label="L${depth}"
  sh "${base_dir}/benchmark/signal_jasmine/run_for_fixed_depth.sh" "$depth_label"

  depth_results="$(ls -t "${base_dir}/results/${proj_name}_"*"_${depth_label}" | head -n 1)"
  depth_logs="$(ls -td "${base_dir}/logs/${proj_name}_"*"_${depth_label}" | head -n 1)"
  cp -f "${depth_logs}/path_oram.out" "${logs_folder}/path_oram_L${depth}.out"
  cp -f "${depth_logs}/loaded_table.out" "${logs_folder}/loaded_table_L${depth}.out"
  cp -f "${depth_logs}/loaded_sharded_table.out" "${logs_folder}/loaded_sharded_table_L${depth}.out"
  sed '/^$/d' "$depth_results" >> "$results_file"
  rm -f "$depth_results"
  rm -rf "$depth_logs"
done

echo "Done. Results: ${results_file}"
echo "Logs: ${logs_folder}"
