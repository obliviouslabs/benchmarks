#!/bin/sh
set -eu

base_dir=$(git rev-parse --show-toplevel)
build_folder="$base_dir/build/cpp_stl"
mkdir -p "$base_dir/build"
staging=$(mktemp -d "$base_dir/build/.cpp-stl-XXXXXX")
trap 'rm -rf "$staging"' EXIT
trap 'exit 1' HUP INT TERM
mkdir "$staging/source"
cp "$base_dir/benchmark/cpp_stl/benchmark_code/stl_bench.cpp" \
   "$base_dir/benchmark/common/common.h" "$staging/source/"
# Local benchmark, no upstream checkout. Fingerprint the actual copied sources,
# including uncommitted edits, rather than labeling them with an old Git commit.
(cd "$staging/source"; sha256sum common.h stl_bench.cpp) | sha256sum | cut -d ' ' -f 1 > "$staging/SOURCE.sha256"
rm -rf "$build_folder"
mv "$staging" "$build_folder"
