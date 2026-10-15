#!/bin/sh
set -eu

base_dir=$(git rev-parse --show-toplevel)
build_folder="$base_dir/build/cpp_stl"
# Keep symbols for disassembly; -g does not disable -O3. No benchmark framework,
# downloads, external libraries, or per-access timing calls are needed.
"${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -march=native -g -Wall -Wextra "$build_folder/source/stl_bench.cpp" -o "$build_folder/cpp_stl_bench"
# "${CXX:-c++}" -std=c++20 -O0 -DNDEBUG -g -Wall -Wextra "$build_folder/source/stl_bench.cpp" -o "$build_folder/cpp_stl_bench"
    
