run_timestamp=$(date +%s)
repo_path=${repo_path:-"/"}
commit_hash="$(git -C ${base_dir}/build/${proj_name}/${repo_path} rev-parse HEAD 2>/dev/null || echo 'unknown')"
build_folder="${base_dir}/build/${proj_name}/"
sources_folder="${base_dir}/benchmark/${proj_name}"

# These should not be used by build.sh, they are intended for run.sh
run_id="${proj_name}_${run_timestamp}_${commit_hash}"
[[ $# -ge 1 ]] && run_id+="_$1"
results_file="${base_dir}/results/${run_id}"
logs_folder="${base_dir}/logs/${run_id}/"