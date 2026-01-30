#!/bin/sh
set -eu

benchmarks='
h2o2_oram
mc_oblivious
meta_oram
olabs_oram
olabs_rostl
signal_icelake
'

ARGC=$#

in_args() {
  needle=$1
  shift
  for x do
    [ "$x" = "$needle" ] && return 0
  done
  return 1
}

run_if_selected() {
  b=$1
  script=$2
  shift 2

  # If no args, run all. If args exist, only run if benchmark name appears.
  if [ "$ARGC" -eq 0 ] || in_args "$b" "$@"; then
    sh "benchmark/$b/$script"
  fi
}

# setups
for b in $benchmarks; do
  run_if_selected "$b" setup.sh "$@"
done

# builds
for b in $benchmarks; do
  run_if_selected "$b" build.sh "$@"
done