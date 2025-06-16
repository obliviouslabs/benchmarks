#!/bin/bash
set -e

rm -rf build
mkdir -p build
mkdir -p results
mkdir -p logs

# Run all inner setups
#
sh benchmark/mc_oblivious/setup.sh
sh benchmark/meta_oram/setup.sh
sh benchmark/olabs_oram/setup.sh
sh benchmark/olabs_rostl/setup.sh
sh benchmark/signal_icelake/setup.sh