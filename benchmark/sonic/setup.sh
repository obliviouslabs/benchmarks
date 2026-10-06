#!/bin/sh
set -eu

base_dir=$(git rev-parse --show-toplevel)
build_folder="$base_dir/build/sonic"
archive_url='https://zenodo.org/records/21522021/files/sonic-usenix26-artifacts_20260717.tar.gz?download=1'
archive_sha256='b92b67c3c5f745331d9329b62467badfef807983d69eda4b798ef1e185955b6c'

# Verify and prepare the release before replacing an existing SONIC build.
mkdir -p "$base_dir/build"
staging=$(mktemp -d "$base_dir/build/.sonic-XXXXXX")
trap 'rm -rf "$staging"' EXIT
trap 'exit 1' HUP INT TERM
curl --fail --location --retry 3 --output "$staging/artifact.tar.gz" "$archive_url"
printf '%s  %s\n' "$archive_sha256" "$staging/artifact.tar.gz" | sha256sum --check
mkdir "$staging/source"
tar -xzf "$staging/artifact.tar.gz" --no-same-owner --strip-components=2 \
    -C "$staging/source" sonic-usenix26-artifacts/sonic
rm "$staging/artifact.tar.gz"
printf '%s\n' "$archive_sha256" > "$staging/SOURCE.sha256"

cp -r "$base_dir/benchmark/sonic/benchmark_code" "$staging/source/benchmarks"
cp "$base_dir/benchmark/common/common.h" "$base_dir/benchmark/common/ramp_latency.h" \
    "$staging/source/benchmarks/"
printf '\nadd_subdirectory(benchmarks)\n' >> "$staging/source/CMakeLists.txt"

rm -rf "$build_folder"
mv "$staging" "$build_folder"
