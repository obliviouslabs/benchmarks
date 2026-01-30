#!/bin/bash
set -e

# sh ./scripts/reset.sh
# sh ./scripts/setup.sh h2o2_oram signal_icelake

### Checklist before this:
# 1) Make sure you have installed the msrp (rust)
# 2) Make sure you have run the setup tools script in olabs_rostl
# 3) Make sure you have installed all the c++ packages required for signal_icelake and olabs_rostl
# 4) Make sure you have docker running

# sh ./benchmark/olabs_oram/run.sh
# sh ./benchmark/signal_icelake/run.sh
# sh ./benchmark/h2o2_oram/run.sh
# sh ./benchmark/olabs_rostl/run.sh
# sh ./benchmark/mc_oblivious/run.sh
# sh ./benchmark/meta_oram/run.sh

# To setup swap:
# sudo dd if=/dev/zero of=/data/swapfile_test bs=1G count=8 status=progress
# sudo chmod 600 /data/swapfile_test
# sudo mkswap /data/swapfile_test
# sudo swapoff -a
# sudo swapon /data/swapfile_test


sudo systemd-run --scope -p MemoryMax=1G -p MemorySwapMax=60G sudo -u $(whoami) sh ./benchmark/signal_icelake/run.sh SWAP1G

# sudo systemd-run --scope -p MemoryMax=1G -p MemorySwapMax=60G sudo -u $(whoami) sh ./benchmark/h2o2_oram/run.sh SWAP1G


# To run this script with memory limits: 
# sudo systemd-run --scope -p MemoryMax=8G -p MemorySwapMax=60G sudo -u $(whoami) sh ./benchmark/__TARGET__/run.sh