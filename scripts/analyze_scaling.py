#!/usr/bin/env -S uv run python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "matplotlib",
#   "pandas",
#   "numpy",
#   "tabulate @ git+https://github.com/astanin/python-tabulate.git"
# ]
# ///
import pandas as pd
from utils import load_df, draw_table, HEADER, SUBHEADER, TITLE, NL, NOTES, HORIZONTAL_LINE

P = load_df()

TITLE("h2o2 ORAM - Scaling with number of cores (8B values)")
w1 = P.loc[
  (P['Key_bytes'] == 8)
  & (P['Value_bytes'] == 56)
  & (P['benchmark_type'] == 'RORAM')
  & (P['Read_latency_us'].notna())
  & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
  & ((P['implementation'] == 'h2o2'))
].sort_index().copy()

for threads in [1,2,4,15,16,32]:
  w1['name'] = w1['sys_lcores'].map(lambda c: f"h2o2-{c:02d}c")

draw_table(w1, 'N', 'Read_latency_us', columns='name')
NL(1)