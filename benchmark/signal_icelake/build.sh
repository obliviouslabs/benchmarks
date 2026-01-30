#!/bin/bash
set -e 

proj_name="signal_icelake"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

cd "${build_folder}/c"

make docker_testsbin
