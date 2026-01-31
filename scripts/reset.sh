#!/bin/bash
set -e

ARGC=$#

if [ "$ARGC" -eq 0 ]; then
  echo "Resetting all benchmarks..."
  rm -rf build
  mkdir -p build
  mkdir -p results
  mkdir -p logs
else
  echo "Resetting selected benchmarks: $@"
  mkdir -p build
  mkdir -p results
  mkdir -p logs
  for b in "$@"; do
    echo "Resetting benchmark: $b"
    rm -rf "build/$b"
  done
fi
