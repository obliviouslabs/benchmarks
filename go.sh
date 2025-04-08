#!/bin/bash
set -e
sh ./scripts/setup.sh

### Checklist before this:
# 1) Make sure you have installed the msrp (rust)
# 2) Make sure you have run the setup tools script in olabs_rods
# 3) Make sure you have installed all the c++ packages required for signal_icelake and olabs_rods
# 4) Make sure you have docker running

sh ./benchmark/meta_oram/run.sh
sh ./benchmark/olabs_rods/run.sh
sh ./benchmark/olabs_oram/run.sh
sh ./benchmark/signal_icelake/run.sh
