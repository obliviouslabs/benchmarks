#!/bin/bash
set -e
sh ./scripts/setup.sh

sh ./benchmark/olabs_rods/run.sh
# sh ./benchmark/olabs_oram/run.sh
# sh ./benchmark/signal_icelake/run.sh
  