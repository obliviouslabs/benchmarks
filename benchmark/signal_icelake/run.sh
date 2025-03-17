#!/bin/bash
set -e 

base_dir=$(git rev-parse --show-toplevel)
commit_hash=$(git -C $base_dir/build/ContactDiscoveryService-Icelake/ rev-parse HEAD)
timestamp=$(date +%s)
run_id=signal_icelake_${commit_hash}_${timestamp}
results_file=$base_dir/results/$run_id
run_folder=$base_dir/build/ContactDiscoveryService-Icelake/c


# Run the tests
cd $run_folder
make docker_tests

# Parse the results
mkdir -p $base_dir/logs/$run_id
cp $run_folder/benchmarks/loaded_table.test.out $base_dir/logs/$run_id/loaded_table.test.out
cp $run_folder/benchmarks/loaded_sharded_table.test.out $base_dir/logs/$run_id/loaded_sharded_table.test.out

echo "" > $results_file
python $base_dir/scripts/parse.py -f $run_folder/benchmarks/loaded_table.test.out >> $results_file
python $base_dir/scripts/parse.py -f $run_folder/benchmarks/loaded_sharded_table.test.out >> $results_file
