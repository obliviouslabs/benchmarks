#!/bin/bash
set -e 

proj_name="signal_jasmine"
repo_path="source"
base_dir=$(git rev-parse --show-toplevel)
. "${base_dir}/scripts/gen_args.sh"

if [ ! -f "${build_folder}/depths" ]; then
  echo "Run benchmark/signal_jasmine/setup.sh before building" >&2
  exit 1
fi

for depth_folder in "${build_folder}"/L*/; do
  echo "Building Jasmine $(basename "$depth_folder")"
  (cd "${depth_folder}/c" && make docker_testsbin)
done
