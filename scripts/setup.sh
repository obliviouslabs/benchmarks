#!/bin/bash
set -e

rm -rf build
mkdir -p build
mkdir -p results
mkdir -p logs

# Run all inner setups
#
sh benchmark/h2o2_oram/setup.sh "$@"
sh benchmark/mc_oblivious/setup.sh "$@"
sh benchmark/meta_oram/setup.sh "$@"
sh benchmark/olabs_oram/setup.sh "$@"
sh benchmark/olabs_rostl/setup.sh "$@"
sh benchmark/signal_icelake/setup.sh "$@"

# Run all inner builds
#
# sh benchmark/h2o2_oram/build.sh "$@"
sh benchmark/mc_oblivious/build.sh "$@"
sh benchmark/meta_oram/build.sh "$@"
sh benchmark/olabs_oram/build.sh "$@"
sh benchmark/olabs_rostl/build.sh "$@"
sh benchmark/signal_icelake/build.sh "$@"