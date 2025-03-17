#!/bin/bash
set -e
sh ./scripts/setup.sh

sh ./benchmark/signal_icelake/run.sh
  