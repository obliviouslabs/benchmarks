#!/bin/bash
set -eu

base_dir=$(git rev-parse --show-toplevel)
map_sizes="${RAMP_MAP_SIZES:-65536 131072 262144 524288 1048576 2097152 4194304 8388608}"
implementations="${RAMP_IMPLEMENTATIONS:-h2o2_oram olabs_oram olabs_oram_sharded olabs_rostl mc_oblivious signal_icelake signal_jasmine}"
run_timestamp=$(date +%s)
output_dir="${RAMP_OUTPUT_DIR:-${base_dir}/logs/ramp_latency_${run_timestamp}}"
mkdir -p "$output_dir"

ran=0
skipped=0

run_one()
{
    implementation=$1
    n=$2
    output="${output_dir}/${implementation}_N${n}.csv"

    echo "Running latency workload: implementation=${implementation} N=${n} workload=${RAMP_WORKLOAD:-ramp}"
    case "$implementation" in
        h2o2_oram)
            binary="${base_dir}/build/h2o2_oram/bin/h2o2_oram_ramp"
            if [ ! -x "$binary" ]; then
                echo "Skipping ${implementation}: build it with benchmark/h2o2_oram/build.sh" >&2
                skipped=$((skipped + 1))
                return
            fi
            "$binary" "$n" "$output"
            ;;
        olabs_oram)
            binary="${base_dir}/build/olabs_oram/build/applications/benchmarks/umap_ramp"
            if [ ! -x "$binary" ]; then
                echo "Skipping ${implementation}: build it with benchmark/olabs_oram/build.sh" >&2
                skipped=$((skipped + 1))
                return
            fi
            "$binary" "$n" "$output"
            ;;
        olabs_oram_sharded)
            binary="${base_dir}/build/olabs_oram/build/applications/benchmarks/umap_sharded_ramp"
            if [ ! -x "$binary" ]; then
                echo "Skipping ${implementation}: build it with benchmark/olabs_oram/build.sh" >&2
                skipped=$((skipped + 1))
                return
            fi
            "$binary" "$n" "$output"
            ;;
        olabs_rostl)
            manifest="${base_dir}/build/olabs_rostl/Cargo.toml"
            if [ ! -f "$manifest" ]; then
                echo "Skipping ${implementation}: run benchmark/olabs_rostl/setup.sh first" >&2
                skipped=$((skipped + 1))
                return
            fi
            cargo run --quiet --profile=maxperf --manifest-path "$manifest" \
                --bin ramp_latency -- "$n" "$output"
            ;;
        mc_oblivious)
            manifest="${base_dir}/build/mc_oblivious/Cargo.toml"
            if [ ! -f "$manifest" ]; then
                echo "Skipping ${implementation}: run benchmark/mc_oblivious/setup.sh first" >&2
                skipped=$((skipped + 1))
                return
            fi
            cargo run --quiet --profile=maxperf --manifest-path "$manifest" \
                --bin ramp_latency -- "$n" "$output"
            ;;
        signal_icelake)
            binary="${base_dir}/build/signal_icelake/c/benchmarks/loaded_table_ramp.test"
            if [ ! -x "$binary" ]; then
                echo "Skipping ${implementation}: build it with benchmark/signal_icelake/build.sh" >&2
                skipped=$((skipped + 1))
                return
            fi
            "$binary" "$n" "$output"
            ;;
        signal_jasmine)
            binary="${base_dir}/build/signal_jasmine/c/benchmarks/loaded_table_ramp.test"
            if [ ! -x "$binary" ]; then
                echo "Skipping ${implementation}: build a fixed depth with benchmark/signal_jasmine/build.sh" >&2
                skipped=$((skipped + 1))
                return
            fi
            "$binary" "$n" "$output"
            ;;
        *)
            echo "Unknown RAMP_IMPLEMENTATIONS entry: ${implementation}" >&2
            exit 2
            ;;
    esac
    ran=$((ran + 1))
}

for n in $map_sizes; do
    case "$n" in
        ''|*[!0-9]*)
            echo "RAMP_MAP_SIZES entries must be positive integers: ${n}" >&2
            exit 2
            ;;
    esac
    if [ "$n" -eq 0 ]; then
        echo "RAMP_MAP_SIZES entries must be greater than zero" >&2
        exit 2
    fi
    for implementation in $implementations; do
        run_one "$implementation" "$n"
    done
done

if [ "$ran" -eq 0 ]; then
    echo "No latency workloads ran; set up and build at least one requested implementation." >&2
    exit 1
fi

echo "Latency workload results: ${output_dir} (${ran} run(s), ${skipped} skipped)"
