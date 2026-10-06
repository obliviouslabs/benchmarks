#!/bin/bash
set -e

if [ -z "${BENCHMARK_LOG_FILE:-}" ]; then
  mkdir -p "$PWD/logs/full_runs"
  BENCHMARK_LOG_FILE=$(mktemp "$PWD/logs/full_runs/$(date +%Y%m%d-%H%M%S)-XXXXXX.jsonl")
  printf 'Full run log: %s\n' "$BENCHMARK_LOG_FILE" >&2
fi
export BENCHMARK_LOG_FILE

sh ./scripts/reset.sh
sh ./scripts/setup.sh

### Checklist before this:
# 1) Make sure you have installed the msrp (rust)
# 2) Make sure you have run the setup tools script in olabs_rostl
# 3) Make sure you have installed all the c++ packages required for signal_icelake and olabs_rostl
# 4) Make sure you have docker running

sh ./benchmark/olabs_oram/run.sh
sh ./benchmark/signal_icelake/run.sh
sh ./benchmark/signal_jasmine/run.sh
sh ./benchmark/h2o2_oram/run.sh
sh ./benchmark/olabs_rostl/run.sh
sh ./benchmark/mc_oblivious/run.sh
sh ./benchmark/meta_oram/run.sh
sh ./benchmark/sonic/run.sh

# Comment out the lines you don't want this run to execute.

# To setup swap:
# sudo dd if=/dev/zero of=/data/swapfile_test bs=1G count=256 status=progress
# sudo chmod 600 /data/swapfile_test
# sudo mkswap /data/swapfile_test
# sudo swapoff -a
# sudo swapon /data/swapfile_test

# systemd-run --user --scope -p MemoryMax=2G -p MemorySwapMax=60G sh ./benchmark/olabs_oram/run.sh SWAP2G
# systemd-run --user --scope -p MemoryMax=1G -p MemorySwapMax=60G sh ./benchmark/signal_icelake/run.sh SWAP1G
systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=60G sh ./benchmark/signal_jasmine/run.sh SWAP512M
systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=60G sh ./benchmark/h2o2_oram/run.sh SWAP512M
systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=60G sh ./benchmark/olabs_oram/run.sh SWAP512M
systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=60G sh ./benchmark/sonic/run.sh SWAP512M

# systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=60G sh ./benchmark/signal_jasmine/run.sh SWAP512M
# systemd-run --user --scope -p MemoryMax=64G -p MemorySwapMax=200G sh ./benchmark/h2o2_oram/run.sh SWAP64G

sh ./scripts/run_ramp_latency.sh
