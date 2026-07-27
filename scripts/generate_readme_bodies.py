#!/usr/bin/env python3
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
VENV_PYTHON = REPO_ROOT / ".venv" / "bin" / "python"

if VENV_PYTHON.exists() and Path(sys.executable) != VENV_PYTHON:
  os.execv(str(VENV_PYTHON), [str(VENV_PYTHON), __file__, *sys.argv[1:]])

import pandas as pd
from utils import load_df, draw_table, HEADER, SUBHEADER, TITLE, NL, NOTES, HORIZONTAL_LINE
from augment import augmentP


README_IMPLEMENTATION = "readme_implementation"
SHARDED_IMPLEMENTATIONS = [
  "Signal_Sharded",
  "Signal_Jasmine_Sharded",
  "olabs_rostl_sharded",
  "olabs_oram_sharded",
]
README_SHARD_COUNTS = [15, 16]
BATCH_SIZES_FOR_4K_README = [4096, 8192, 65536]


def format_power_of_two(value):
  return f"$2^{{{int(value).bit_length() - 1}}}$"


def format_batch_size(value):
  return f"{int(value):07d}"


def format_batch_sizes(values):
  def maybe_k(value):
    value = int(value)
    if value % 1024 == 0:
      return f"{value // 1024}K"
    return str(value)
  return ", ".join([maybe_k(value) for value in values])


def readme_implementation_label(implementation, benchmark_type=None):
  if implementation == "Signal":
    return "Signal-Old"
  if implementation == "Signal_Sharded":
    return "Signal-Old-Sharded"
  if implementation == "Signal_Jasmine":
    return "Signal-Jasmine"
  if implementation == "Signal_Jasmine_Sharded":
    return "Signal-Jasmine-Sharded"
  if implementation == "olabs_oram" and benchmark_type == "UnorderedMap":
    return "olabs_umap"
  if implementation == "olabs_oram_sharded":
    return "olabs_umap_sharded"
  if implementation == "olabs_oram_shortkv":
    return "olabs_umap_shortkv"
  return implementation


def add_readme_columns(df):
  df = df.copy()
  df[README_IMPLEMENTATION] = df.apply(
    lambda row: readme_implementation_label(row["implementation"], row["benchmark_type"]),
    axis=1,
  )
  return df


def draw_readme_table(data, x_name, y_name, **kwargs):
  draw_table(data, x_name, y_name, columns=README_IMPLEMENTATION, **kwargs)


def is_readme_sharded_row(df):
  return df["Shards"].isin(README_SHARD_COUNTS)


P = load_df()
P = augmentP(P.copy())
P = add_readme_columns(P)

HEADER("ORAM")
SUBHEADER("NRORAM")
TITLE("NRORAM - Sequential Read Latency (us) for 8B Values")
w1 = P.loc[
  (P['Key_bytes'] == 8)
  & (P['Value_bytes'] == 8)
  & (P['benchmark_type'] == 'NRORAM')
  & (P['Read_latency_us'].notna())
  & (P['N'] >= (1<<10)) & (P['N'] <= (1<<28))
].sort_index().copy()
w1['N'] = w1['N'].map(format_power_of_two)
draw_readme_table(w1, 'N', 'Read_latency_us')
NL(2)

SUBHEADER("RORAM")
for value_bytes in [8, 56]:
  TITLE(f"RORAM - Sequential Read Latency (us) for {value_bytes}B Values")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == value_bytes) 
    & (P['benchmark_type'] == 'RORAM') 
    & (P['Read_latency_us'].notna()) 
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<28))
  ].sort_index().copy()
  w1['N'] = w1['N'].map(format_power_of_two)
  draw_readme_table(w1, 'N', 'Read_latency_us')

HORIZONTAL_LINE()

HEADER("Unordered Map")
SUBHEADER("Unordered Map - Sequential Queries")
for (key_bytes,value_bytes) in [(8, 8), (8, 56), (32, 32)]:
  TITLE(f"UnorderedMap - Sequential Read Latency (us) for {key_bytes}B keys, {value_bytes}B Values")
  w1 = P.loc[
    (P['Key_bytes'] == key_bytes)
    & (P['Value_bytes'] == value_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_latency_us'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & (P['Shards'].isna())
  ].sort_index().copy()
  w1['N'] = w1['N'].map(format_power_of_two)
  draw_readme_table(w1, 'N', 'Get_latency_us')
  NL(2)


SUBHEADER("Unordered Map - Batched Queries")
for value_bytes in [8, 56]:
  title_suffix = format_batch_sizes(BATCH_SIZES_FOR_4K_README)
  TITLE(f"UnorderedMap - Batch Read Latency (us) for 8B keys, {value_bytes}B Values ({title_suffix} queries/batch, 32 threads)")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == value_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_latency_us'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & (P['Batch_size'].isin(BATCH_SIZES_FOR_4K_README))
    & is_readme_sharded_row(P)
  ].copy()
  w1['N'] = w1['N'].map(format_power_of_two)
  w1["name"] = w1[README_IMPLEMENTATION].astype(str) + "-" + w1["Batch_size"].map(format_batch_size)
  draw_table(w1, 'N', 'Get_latency_us', columns='name')
  NL(1)

  TITLE(f"UnorderedMap - Batch Read Throughput (qps) for 8B keys, {value_bytes}B Values ({title_suffix} queries/batch, 32 threads)")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == value_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_throughput_qps'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & (P['Batch_size'].isin(BATCH_SIZES_FOR_4K_README))
    & is_readme_sharded_row(P)
  ].sort_index().copy()
  w1['N'] = w1['N'].map(format_power_of_two)
  w1["name"] = w1[README_IMPLEMENTATION].astype(str) + "-" + w1["Batch_size"].map(format_batch_size)
  draw_table(w1, 'N', 'Get_throughput_qps', columns='name', highlight=1)
  NL(2)



SUBHEADER("Unordered Map - Scaling with Batch Size")
for implementation in SHARDED_IMPLEMENTATIONS:
  implementation_label = readme_implementation_label(implementation, "UnorderedMap")
  TITLE(f"Unordered Map - Scaling with Batch Size - Read Throughput (qps) for 8B keys, 56B Values ({implementation_label})")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == 56)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_throughput_qps'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & is_readme_sharded_row(P)
    & (P['implementation'] == implementation)
  ].sort_index().copy()
  w1['N'] = w1['N'].map(format_power_of_two)
  w1["name"] = w1[README_IMPLEMENTATION].astype(str) + "-" + w1["Batch_size"].map(format_batch_size)
  draw_table(w1, 'N', 'Get_throughput_qps', columns='name', highlight=1)

  TITLE(f"Unordered Map - Scaling with Batch Size - Read Latency (us) for 8B keys, 56B Values ({implementation_label})")
  draw_table(w1, 'N', 'Get_latency_us', columns='name', highlight=-1)


  
SUBHEADER("Unordered Map - Query cost breakdown")

TITLE(f"Unordered Map - olabs_sharded - Load balance percentage for 8B keys, 56B Values")
key_bytes = 8
value_bytes = 56

w1 = P.loc[
  (P['Key_bytes'] == key_bytes)
  & (P['Value_bytes'] == value_bytes)
  & (P['benchmark_type'] == 'UnorderedMap')
  & (P['implementation'] == 'olabs_oram_sharded')
  & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
  & (P['Shards'] == 15)
].sort_index().copy()

w1['N'] = w1['N'].map(format_power_of_two)
w1['Batch_sz'] = w1['Batch_size'].map(format_batch_size)
draw_table(w1, 'N', 'Percentage_batch_lb', columns='Batch_sz',highlight=0)


HEADER("Load Balancer")
key_bytes = 8
value_bytes = 8
w1 = P.loc[
  (P['Key_bytes'] == key_bytes)
  & (P['Value_bytes'] == value_bytes)
  # & (P['Shards'] == 15)
  & (P['benchmark_type'] == 'LOADBALANCE')
  & (P['Latency_us'].notna())
  & (P['B'] >= (1<<10)) & (P['B'] <= (1<<26))
].sort_index().copy()
w1['B'] = w1['B'].apply(lambda x: f"{x:05d}")
draw_table(w1, 'B', 'Latency_us', highlight=-1)
