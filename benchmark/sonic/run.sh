#!/bin/sh
set -eu

proj_name="sonic"
base_dir=$(git rev-parse --show-toplevel)
# The upstream release is an archive, not a Git checkout.
read -r source_revision < "$base_dir/build/sonic/SOURCE.sha256"
. "$base_dir/scripts/gen_args.sh"

binaries="sonic_nroram_bench sonic_pmchain_bench"
for binary in $binaries; do
    if [ ! -x "${build_folder}/build/bin/$binary" ]; then
        echo "SONIC binary missing: $binary; run sh scripts/setup.sh sonic first." >&2
        exit 1
    fi
done
mkdir -p "$logs_folder" "$base_dir/results"

# Keep streaming output without letting tee hide a launcher failure.
(
    status=0
    for binary in $binaries; do
        "${build_folder}/build/bin/$binary" || status=$?
    done
    printf '%s\n' "$status" > "${logs_folder}/exit_status"
) 2>&1 | tee "${logs_folder}/sonic.log"
uv run "$base_dir/scripts/parse.py" -f "${logs_folder}/sonic.log" > "$results_file"
read -r status < "${logs_folder}/exit_status"
exit "$status"
