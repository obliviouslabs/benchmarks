#!/bin/bash
set -e

rm -rf build
mkdir -p build
mkdir -p results
mkdir -p logs

# Run all inner setups
#
sh benchmark/signal_icelake/setup.sh
sh benchmark/olabs_oram/setup.sh
sh benchmark/olabs_rods/setup.sh
