#!/bin/bash
set -e 

base_dir=$(git rev-parse --show-toplevel)
commit_hash=$(git -C $base_dir/build/ContactDiscoveryService-Icelake/ rev-parse HEAD)
results_file=$base_dir/results/signal_icelake_$commit_hash
run_folder=$base_dir/build/ContactDiscoveryService-Icelake/c


# Run the tests
cd $run_folder
make docker_tests

# Parse the results
mkdir -p $base_dir/logs/$commit_hash
cp $run_folder/loaded_table.test.out $base_dir/logs/$commit_hash/loaded_table.test.out
cp $run_folder/loaded_sharded_table.test.out $base_dir/logs/$commit_hash/loaded_sharded_table.test.out

echo "" > $results_file
python ./scripts/parse.py -f $run_folder/loaded_table.test.out >> $results_file
python ./scripts/parse.py -f $run_folder/loaded_sharded_table.test.out >> $results_file
