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
w1['N'] = w1.apply(lambda x: f"$2^{{{len(bin(x['N'])[3:])}}}$", axis=1)
draw_table(w1, 'N', 'Read_latency_us')
NL(2)

SUBHEADER("RORAM")
TITLE("RORAM - Sequential Read Latency (us) for 8B Values")
w1 = P.loc[
  (P['Key_bytes'] == 8)
  & (P['Value_bytes'] == 8) 
  & (P['benchmark_type'] == 'RORAM') 
  & (P['Read_latency_us'].notna()) 
  & (P['N'] >= (1<<10)) & (P['N'] <= (1<<28))
].sort_index().copy()
w2 = w1.copy().loc[(P['implementation'] == 'h2o2') & (P['sys_lcores'] == 1)]
w3 = w1.copy().loc[(P['implementation'] == 'h2o2') & (P['sys_lcores'] == 32)]
w3.loc[w3['implementation'] == 'h2o2', 'implementation'] = 'h2o2-32'
w1 = w1.loc[(P['implementation'] != 'h2o2')]
w1 = pd.concat([w1, w2, w3], ignore_index=True)
w1['N'] = w1.apply(lambda x: f"$2^{{{len(bin(x['N'])[3:])}}}$", axis=1)
draw_table(w1, 'N', 'Read_latency_us')
NOTES("""
> [!info]
> All implementations are sequential, except H2O2-32c which runs with 32 cores. 
> H2O2 latency is amortized over N operations, the maximum latency is `N * avg_latency`.
>
""")

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
  w2 = w1.copy().loc[(P['implementation'] == 'h2o2') & (P['sys_lcores'] == 1)]
  w1 = w1.loc[(P['implementation'] != 'h2o2')]
  w1 = pd.concat([w1, w2], ignore_index=True)
  w1['N'] = w1.apply(lambda x: f"$2^{{{len(bin(x['N'])[3:])}}}$", axis=1)
  draw_table(w1, 'N', 'Get_latency_us')
  NL(2)


SUBHEADER("Unordered Map - Batched Queries")
for value_bytes in [8, 56]:
  TITLE(f"UnorderedMap - Batch Read Latency (us) for 8B keys, {value_bytes}B Values, 4K queries/batch (32 threads)")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == value_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_latency_us'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
  ].copy()
  
  w2 = w1.loc[(P['Batch_size'] == 4096) & (P['Shards'] == 15)].copy()
  w3 = w1.loc[(P['implementation'] == 'h2o2') & (P['sys_lcores'] == 32)].copy()
  w3['Get_latency_us'] *= 4096 # adjust for batch size
  w1 = pd.concat([w2, w3], ignore_index=True)
  w1['N'] = w1.apply(lambda x: f"$2^{{{len(bin(x['N'])[3:])}}}$", axis=1)
  draw_table(w1, 'N', 'Get_latency_us')
  NL(1)

  TITLE(f"UnorderedMap - Batch Read Throughput (qps) for 8B keys, {value_bytes}B Values, 4K queries/batch (32 threads)")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == value_bytes)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_throughput_qps'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    &(
      (P['Shards'] == 15) 
     |((P['implementation'] == 'h2o2') & (P['sys_lcores'] == 32)))
  ].sort_index().copy()
  w1['N'] = w1.apply(lambda x: f"$2^{{{len(bin(x['N'])[3:])}}}$", axis=1)
  draw_table(w1, 'N', 'Get_throughput_qps', highlight=1)
  NL(2)



SUBHEADER("Unordered Map - Scaling with Batch Size")
for implementation in ['Signal_Sharded', 'olabs_rostl_sharded', 'olabs_oram_sharded']:
  TITLE(f"Unordered Map - Scaling with Batch Size - Read Throughput (qps) for 8B keys, 56B Values ({implementation})")
  w1 = P.loc[
    (P['Key_bytes'] == 8)
    & (P['Value_bytes'] == 56)
    & (P['benchmark_type'] == 'UnorderedMap')
    & (P['Get_throughput_qps'].notna())
    & (P['N'] >= (1<<10)) & (P['N'] <= (1<<26))
    & (P['Shards'] == 15)
    & (P['implementation'] == implementation)
  ].sort_index().copy()
  w1['N'] = w1.apply(lambda x: f"$2^{{{len(bin(x['N'])[3:])}}}$", axis=1)
  w1["name"] = w1.apply(lambda r: f"{r['implementation']}-{r['Batch_size']:07d}", axis=1)
  draw_table(w1, 'N', 'Get_throughput_qps', columns='name', highlight=1)
  