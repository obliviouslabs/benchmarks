#!/bin/bash
set -e
sh ./scripts/setup.sh

### Checklist before this:
# 1) Make sure you have installed the msrp (rust)
# 2) Make sure you have run the setup tools script in olabs_rostl
# 3) Make sure you have installed all the c++ packages required for signal_icelake and olabs_rostl
# 4) Make sure you have docker running

sh ./benchmark/olabs_oram/run.sh
sh ./benchmark/signal_icelake/run.sh
sh ./benchmark/h2o2_oram/run.sh
sh ./benchmark/olabs_rostl/run.sh
sh ./benchmark/mc_oblivious/run.sh
sh ./benchmark/meta_oram/run.sh
