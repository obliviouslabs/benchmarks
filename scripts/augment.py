import pandas as pd

from utils import get_percentile_factor

def augmentP(P):
  # For olabs_oram_sharded, use LOADBALANCE benchmark to compute cost of load balancing.
  for idx, row in P.loc[
        (P['benchmark_type'] == 'UnorderedMap') 
      & (P['implementation'] == 'olabs_oram_sharded')
      & (P['Key_bytes'] == 8)
    ].iterrows():
    lb_row = P.loc[
      (P['benchmark_type'] == 'LOADBALANCE')
      & (P['implementation'] == 'olabs_oram')
      & (P['Key_bytes'] == row['Key_bytes'])
      # & (P['Value_bytes'] == row['Value_bytes'])
      # & (P['Shards'] == row['Shards'])
      & (P['B'] == row['Batch_size'])
    ]
    if not lb_row.empty:
      lb_time = lb_row.iloc[0]['Latency_us']
      accurate_latency = row['Get_burst_latency_us'] + row['Get_writeback_latency_us']
      P.at[idx, 'Get_lb_latency_us'] = lb_time
      P.at[idx, 'Percentage_batch_lb'] = lb_time / accurate_latency
      P.at[idx, 'Percentage_batch_online'] = (row['Get_burst_latency_us'] / accurate_latency)
      P.at[idx, 'Percentage_batch_offline'] = (row['Get_writeback_latency_us'] / accurate_latency)
    else:
      if row['Batch_size'] != 15:
        assert False, f"Missing LOADBALANCE row for olabs_oram_sharded umap: {row.to_dict()}"
  
  # Insert computed rows for h2o2 map cost from h2o2 oram cost
  new_rows = []
  for idx, row in P.loc[
      (P['benchmark_type'] == 'RORAM')
    & (P['implementation'] == 'h2o2')
    & (P['Key_bytes'] == 8)
  ].iterrows():
    new_row = row.copy()
    new_row['benchmark_type'] = 'UnorderedMap'
    new_row['Get_latency_us'] = row['Read_latency_us']
    new_row['Get_max_latency_us'] = row['Read_max_latency_us']
    new_row['Get_throughput_qps'] = row['Read_throughput_qps']
    new_row['Initialization_time_us'] = row['N'] * row['Read_latency_us']

    new_rows.append(new_row)
    new_row = new_row.copy()
    if row['Value_bytes'] == 40:
      new_row['Value_bytes'] = 32
      new_row['Key_bytes'] = 32
    new_rows.append(new_row)

  add = pd.DataFrame(new_rows).dropna(axis=1, how="all")
  P = pd.concat([P.copy(), add], ignore_index=True)
  new_rows = []

  # Insert computed 90% percentiles for h2o2
  for idx, row in P.loc[
    (P['implementation'] == 'h2o2')
  ].iterrows():
    if not pd.isna(row['Get_latency_us']):
      P.at[idx, 'Get_90pct_latency_us'] = row['Get_latency_us'] * get_percentile_factor(0.90)
    if not pd.isna(row['Read_latency_us']):
      P.at[idx, 'Read_90pct_latency_us'] = row['Read_latency_us'] * get_percentile_factor(0.90)

  # Insert computed rows for signal icelake cost for 32b keys/values
  for idx, row in P.loc[
      (P['benchmark_type'] == 'UnorderedMap')
    & (
      (P['implementation'] == 'Signal')
      | (P['implementation'] == 'Signal_Sharded')
    )
    & (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == 56)
  ].iterrows():
    new_row = row.copy()
    new_row['Key_bytes'] = 32
    new_row['Value_bytes'] = 32
    new_rows.append(new_row)

  add = pd.DataFrame(new_rows).dropna(axis=1, how="all")
  P = pd.concat([P.copy(), add], ignore_index=True)
  return P