#!/usr/bin/env -S uv run python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "matplotlib",
#   "pandas",
#   "numpy",
# ]
# ///
import pandas as pd
from utils import get_percentile_factor, load_df, draw_figure
from augment import augmentP

P = load_df()
P = augmentP(P.copy())


# NRORAM plots
w1 = P.loc[
  (P['Key_bytes'] == 8)
  & (P['Value_bytes'] == 8)
  & (P['benchmark_type'] == 'NRORAM')
  & (P['N'] >= (1<<10)) & (P['N'] <= (1<<28))
].sort_index().copy()
common_args = {
  "xlabel": 'N',
  "xscale": 'log',
  "yscale": 'log',
  "xlim": (1000, 300_000_000),
  "tight_layout": {},
  "legend": {},
  "grid": True
}

draw_figure(w1, 'N', 'Read_latency_us',
            "NRORAM - Read Latency (8b value)",
            ylabel='$\\mu s$',
            **common_args
)
draw_figure(w1, 'N', 'Initialization_zeroed_time_us',
            "NRORAM - Initialization Time (8b value, Zeroed)",
            ylabel='$\\mu s$',
            **common_args
)
draw_figure(w1, 'N', 'Memory_kb',
            "NRORAM - Memory Usage (8b value)",
            ylabel='KB',
            **common_args
)
draw_figure(w1, 'N', 'Read_throughput_qps',
            "NRORAM - Read Throughput (8b value)",
            ylabel='Queries per second',
            **common_args
)


# RORAM plots
for val_bytes in [8, 32, 56]:
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == val_bytes)
    & (P['benchmark_type'] == 'RORAM')
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<28))
  ].sort_index().copy()

  w2 = w1.copy().loc[(w1['implementation'] == 'h2o2') & (w1['sys_lcores'] == 32)]
  w2['implementation'] = 'h2o2-32'
  w2_avg = w2.copy()
  w2_percentile = w2.copy()
  w2_percentile['Read_latency_us'] = w2_percentile['Read_90pct_latency_us']
  w2_avg['implementation'] = 'h2o2 (average)'
  w2_percentile['implementation'] = 'h2o2 (0.90 percentile)'

  w2_1 = w1.copy().loc[(w1['implementation'] == 'h2o2') & (w1['sys_lcores'] == 1)]
  w2_1['implementation'] = 'h2o2-1'
  w2_1_avg = w2_1.copy()
  w2_1_percentile = w2_1.copy()
  w2_1_percentile['Read_latency_us'] = w2_1_percentile['Read_90pct_latency_us']
  w2_1_avg['implementation'] = 'h2o2-1 (average)'
  w2_1_percentile['implementation'] = 'h2o2-1 (0.90 percentile)'

  w3 = w1.copy().loc[(P['implementation'] != 'h2o2')]


  w1 = pd.concat([w2_avg, w2_percentile, w2_1_avg, w2_1_percentile, w3], ignore_index=True)
  w1p = pd.concat([w2, w2_1, w3], ignore_index=True)
  # w1 = pd.concat([w2_avg, w2_percentile, w3], ignore_index=True)
  # w1_i = pd.concat([w2, w3, w4], ignore_index=True)


  common_args = {
    "xlabel": 'N',
    "xscale": 'log',
    "yscale": 'log',
    "xlim": (1000, 300_000_000),
    "tight_layout": {},
    "legend": {},
    "grid": True
  }

  draw_figure(w1, 'N', 'Read_latency_us',
              f"ORAM - Read Latency ({val_bytes}b value)",
              ylabel='$\\mu s$',
              **common_args
  )
  draw_figure(w1p, 'N', 'Initialization_zeroed_time_us',
              f"ORAM - Initialization Time ({val_bytes}b value, Zeroed)",
              ylabel='$\\mu s$',
              **common_args
  )
  draw_figure(w1p, 'N', 'Memory_kb',
              f"ORAM - Memory Usage ({val_bytes}b value)",
              ylabel='KB',
              **common_args
  )
  draw_figure(w1p, 'N', 'Read_throughput_qps',
              f"ORAM - Read Throughput ({val_bytes}b value)",
              ylabel='Queries per second',
              **common_args
  )


# UMAP - Sequential
for (key_bytes, val_bytes) in [(8,8), (8,56), (32, 32)]:
  w1 = P.loc[
    (P['Key_bytes'] == key_bytes)
    & (P['Value_bytes'] == val_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & (P['Shards'].isna())
    & (P['implementation'] != 'mc_oblivious') # too slow, makes graphs ugly
  ].sort_index().copy()
  w2 = w1.copy().loc[(w1['implementation'] == 'h2o2') & (w1['sys_lcores'] == 32)]
  w2['implementation'] = 'h2o2-32'
  w2_avg = w2.copy()
  w2_percentile = w2.copy()
  w2_percentile['Get_latency_us'] = w2_percentile['Get_90pct_latency_us']
  w2_avg['implementation'] = 'h2o2-32 (average)'
  w2_percentile['implementation'] = 'h2o2-32 (0.90 percentile)'

  w2_1 = w1.copy().loc[(w1['implementation'] == 'h2o2') & (w1['sys_lcores'] == 1)]
  w2_1['implementation'] = 'h2o2-1'
  w2_1_avg = w2_1.copy()
  w2_1_percentile = w2_1.copy()
  w2_1_percentile['Get_latency_us'] = w2_1_percentile['Get_90pct_latency_us']
  w2_1_avg['implementation'] = 'h2o2-1 (average)'
  w2_1_percentile['implementation'] = 'h2o2-1 (0.90 percentile)'

  w3 = w1.copy().loc[(P['implementation'] != 'h2o2')]

  # Add Naive Initialization time (N * insertion_time) to olabs_oram
  w4 = w1.copy().loc[(P['implementation'] == 'olabs_oram') | (P['implementation'] == 'olabs_oram_shortkv')]
  w4['Initialization_time_us'] = w4['N'] * w4['Get_latency_us'] * 2
  w4["implementation"] = w4.apply(lambda r: f"{r['implementation']}-naive", axis=1)
  
  # w1 = pd.concat([w2_avg, w3], ignore_index=True)
  w1 = pd.concat([w2_avg, w2_percentile, w2_1_avg, w2_1_percentile, w3], ignore_index=True)
  w1i = pd.concat([w2, w2_1, w3, w4], ignore_index=True)
  w1p = pd.concat([w2, w2_1, w3], ignore_index=True)


  common_args = {
    "xlabel": 'N',
    "xscale": 'log',
    "yscale": 'log',
    "xlim": (1000, 100_000_000),
    "tight_layout": {},
    "legend": {},
    "grid": True
  }

  draw_figure(w1, 'N', 'Get_latency_us',
              f"UMAP - Get Latency ({key_bytes}b key, {val_bytes}b value)",
              ylabel='$\\mu s$',
              **common_args
  )
  draw_figure(w1i, 'N', 'Initialization_time_us',
              f"UMAP - Initialization Time ({key_bytes}b key, {val_bytes}b value)",
              ylabel='$\\mu s$',
              **common_args
  )
  draw_figure(w1p, 'N', 'Memory_kb',
              f"UMAP - Memory Usage ({key_bytes}b key, {val_bytes}b value)",
              ylabel='KB',
              **common_args
  )
  draw_figure(w1p, 'N', 'Get_throughput_qps',
              f"UMAP - Get Throughput ({key_bytes}b key, {val_bytes}b value)",
              ylabel='Queries per second',
              **common_args
  )


# UMAP - Batched for different batch sizes
for batch_size in [1024,4096,8192,65536,1048576]:
  batch_size_name = f"{batch_size//1024}K" if batch_size >= 1024 else str(batch_size)
  if batch_size//1024 >= 1024:
    batch_size_name = f"{batch_size//(1024*1024)}M"
  for val_bytes in [8, 56]:
    w1 = P.loc[
      (P['Key_bytes'] == 8)
      & (P['Value_bytes'] == val_bytes)
      & (P['benchmark_type'] == 'UnorderedMap')
      & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    ].sort_index().copy()
    w2 = w1.loc[
      (w1['Shards'] == 15)
    & (w1['Batch_size'] == batch_size)
    ]
    w3 = w1.loc[w1['implementation'] == 'h2o2'].copy()
    w3_32 = w3.loc[w3['sys_lcores'] == 32].copy()
    w3_32['implementation'] = 'h2o2-32'
    w3_1 = w3.loc[w3['sys_lcores'] == 1].copy()
    w3_1['implementation'] = 'h2o2-1'
    w3 = pd.concat([w3_32, w3_1], ignore_index=True)
    w3['Get_latency_us'] *= batch_size
    w1 = pd.concat([w2, w3], ignore_index=True)

    common_args = {
      "xlabel": 'N',
      "xscale": 'log',
      "yscale": 'log',
      "xlim": (1000, 100_000_000),
      "tight_layout": {},
      "legend": {},
      "grid": True
    }

    if batch_size < 1048576:
      common_args["ylim"] = (1000, 1_000_000)

    draw_figure(w1, 'N', 'Get_latency_us',
                f"UMAP - Batch Get Latency - {batch_size_name} batch (8b key, {val_bytes}b value)",
                ylabel='$\\mu s$',
                **common_args
    )

    if batch_size < 1048576:
      del common_args["ylim"]

    draw_figure(w1, 'N', 'Initialization_time_us',
                f"UMAP - Batch Initialization Time - {batch_size_name} batch (8b key, {val_bytes}b value)",
                ylabel='$\\mu s$',
                **common_args
    )
    draw_figure(w1, 'N', 'Memory_kb',
                f"UMAP - Batch Memory Usage - {batch_size_name} batch (8b key, {val_bytes}b value)",
                ylabel='KB',
                **common_args
    )
    draw_figure(w1, 'N', 'Get_throughput_qps',
                f"UMAP - Batch Get Throughput - {batch_size_name} batch (8b key, {val_bytes}b value)",
                ylabel='Queries per second',
                **common_args
    )

# UMAP - Batched performance scaling with batch size
for i, implementation in enumerate(['Signal_Sharded', 'olabs_rostl_sharded', 'olabs_oram_sharded']):
  for val_bytes in [56]:
    w1 = P.loc[
      (P['Key_bytes'] == 8)
      & (P['Value_bytes'] == val_bytes)
      & (P['benchmark_type'] == 'UnorderedMap')
      & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
      & (P['Shards'] == 15)
      & (P['implementation'] == implementation)
    ].sort_index().copy()
    w1["name"] = w1.apply(lambda r: f"{r['implementation']}-{r['Batch_size']}", axis=1)

    common_args = {
      "xlabel": 'N',
      "xscale": 'log',
      "yscale": 'log',
      "xlim": (1000, 100_000_000),
      "tight_layout": {},
      "legend": {},
      "grid": True
    }

    draw_figure(w1, 'N', 'Get_latency_us',
      f"UMAP - Batch Get Latency vs batch size (8b key, {val_bytes}b value) {i}",
      'name',
      ylabel='$\\mu s$',
      **common_args
    )

    draw_figure(w1, 'N', 'Get_throughput_qps',
      f"UMAP - Batch Get Throughput vs batch size (8b key, {val_bytes}b value) {i}",
      'name',
      ylabel='Queries per second',
      **common_args
    )

# UMAP - Batched performance at infinite batch size
for key_bytes, val_bytes in [(8, 8), (32, 32), (8, 56)]:
  w1 = P.loc[
    (P['Key_bytes'] == key_bytes)
    & (P['Value_bytes'] == val_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & (
      (P['Shards'] == 15)
     |((P['implementation'] == 'h2o2'))
    )
  ].sort_index().copy()

  best = (
    w1.loc[w1.groupby(['implementation', 'N'])['Get_throughput_qps'].idxmax()]
      .sort_values(['implementation', 'N'])
      .reset_index(drop=True)
  )
  w1 = best[['implementation', 'N', 'Batch_size', 'Get_throughput_qps']]

  common_args = {
    "xlabel": 'N',
    "xscale": 'log',
    "yscale": 'log',
    "xlim": (1000, 100_000_000),
    "ylim": (10_000, 10_000_000),
    "tight_layout": {},
    "legend": {},
    "grid": True
  }

  draw_figure(w1, 'N', 'Get_throughput_qps',
    f"UMAP - Batch Get Throughput - stress load ({key_bytes}b key, {val_bytes}b value)",
    "implementation",
    ylabel='Queries per second',
    **common_args
  )

w1 = P.loc[
  (P['Key_bytes'] == 8)
  & (P['Value_bytes'] == 56)
  & (P['benchmark_type'] == 'UnorderedMap')
  & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
  & (
    (P['Shards'] == 15)
   |((P['implementation'] == 'olabs_oram_sharded'))
  )
].sort_index().copy()

common_args = {
  "xlabel": 'N',
  "xscale": 'log',
  # "yscale": 'log',
  "xlim": (1000, 100_000_000),
  "ylim": (0, 1),
  "tight_layout": {},
  "legend": {},
  "grid": True
}



draw_figure(w1, 'N', 'Percentage_batch_lb',
  f"UMAP - Batch Breakdown - Load Balancing (8b key, 56b value)",
  "Batch_size",
  ylabel='Fraction of time spent in load balancing vs Batch Size',
  **common_args
)

draw_figure(w1, 'N', 'Percentage_batch_offline',
  f"UMAP - Batch Breakdown - Offline (8b key, 56b value)",
  "Batch_size",
  ylabel='Fraction of time spent for query response vs Batch Size',
  **common_args
)

draw_figure(w1, 'N', 'Percentage_batch_online',
  f"UMAP - Batch Breakdown - Online  (8b key, 56b value)",
  "Batch_size",
  ylabel='Fraction of time spent in maintenance vs Batch Size',
  **common_args
)

