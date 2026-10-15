#!/bin/sh
set -eu

proj_name="cpp_stl"
base_dir=$(git rev-parse --show-toplevel)
binary="$base_dir/build/cpp_stl/cpp_stl_bench"
if [ ! -x "$binary" ] || [ ! -r "$base_dir/build/cpp_stl/SOURCE.sha256" ]; then
    echo "STL binary missing: run sh scripts/setup.sh cpp_stl first." >&2
    exit 1
fi
read -r source_revision < "$base_dir/build/cpp_stl/SOURCE.sha256"
. "$base_dir/scripts/gen_args.sh"
mkdir -p "$logs_folder" "$base_dir/results"

# Preserve executable failures through tee, just as in the native SONIC launcher.
(
    status=0
    "$binary" || status=$?
    printf '%s\n' "$status" > "${logs_folder}/exit_status"
) 2>&1 | tee "${logs_folder}/cpp_stl.log"
uv run "$base_dir/scripts/parse.py" -f "${logs_folder}/cpp_stl.log" > "$results_file"
read -r status < "${logs_folder}/exit_status"
exit "$status"
