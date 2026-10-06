run_timestamp=$(date +%s)
repo_path=${repo_path:-"/"}
# Release archives can supply their checksum instead of a Git commit.
commit_hash="${source_revision:-$(git -C "${base_dir}/build/${proj_name}/${repo_path}" rev-parse HEAD 2>/dev/null || echo 'unknown')}"
build_folder="${base_dir}/build/${proj_name}/"
sources_folder="${base_dir}/benchmark/${proj_name}"

# These should not be used by build.sh, they are intended for run.sh
run_id="${proj_name}_${run_timestamp}_${commit_hash}"

if [ "$#" -ge 1 ]; then
  run_id="${run_id}_$1"
fi
results_file="${base_dir}/results/${run_id}"
logs_folder="${base_dir}/logs/${run_id}/"
export BENCHMARK_PROJECT="$proj_name" BENCHMARK_RUN="$run_id" BENCHMARK_VARIANT="${1:-}"
