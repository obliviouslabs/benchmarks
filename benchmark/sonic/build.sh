#!/bin/sh
set -eu

base_dir=$(git rev-parse --show-toplevel)
build_folder="$base_dir/build/sonic"

# Upstream requires Clang and C++20. Native mode needs no SGX SDK or devices.
CC=clang CXX=clang++ cmake -S "$build_folder/source" -B "$build_folder/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DSONIC_PLATFORM=native \
    -DSONIC_BUILD_APPLICATIONS=OFF -DSONIC_BUILD_DISTRIBUTED=OFF \
    -DSONIC_CRYPTO_BACKEND=openssl -DSONIC_ENABLE_LOGGING=OFF
cmake --build "$build_folder/build" --target sonic_nroram_bench sonic_pmchain_bench sonic_pmchain_ramp \
    --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-$(getconf _NPROCESSORS_ONLN)}"
