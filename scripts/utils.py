import json
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
import pandas as pd
from plot_config import files


def simplify_columns(df: pd.DataFrame) -> pd.DataFrame:
  for col in df.columns:
    # Try turning into integer:
    for i, row in enumerate(df[col]):
      try:
        if pd.isna(row):
          continue
        if row is None or row.strip() == "":
          assert False 
        w = int(row)
        if str(int(row)) != str(row):
          raise Exception(f"'{str(int(row))}' != '{str(row)}'")
      except Exception as e:
        # print(f"Column {col} is not integer: {row}, at row {i}")
        # print(e)
        break
    else:
      df[col] = df[col].astype(pd.Int64Dtype())
      continue

    # Try turning into float64:
    for row in df[col]:
      try:
        if pd.isna(row):
          continue
        if row is None or row.strip() == "":
          assert False
        w = float(row)
      except Exception as e:
        # print(f"Column {col} is not float64: {row}")
        # print(e)
        break
    else:
      df[col] = df[col].astype(pd.Float64Dtype())
      continue
    df[col] = df[col].astype(str)
    # print(f"Column {col} converted to str")
  return df

def gather_points_from_files(files) -> pd.DataFrame:
  lines = []
  for file in files:
    with open(file, "r") as f:
      lines += f.readlines()
  
  pts = list()
  for line in lines:
    try:
      if line.startswith("#") or line.strip() == "": continue
      pt = json.loads(line)
      assert 'N' in pt.keys(), f"N not in pt keys: {pt.keys()}"
      assert 'implementation' in pt.keys(), f"implementation not in pt keys: {pt.keys()}"
      assert 'benchmark_type' in pt.keys(), f"benchmark_type not in pt keys: {pt.keys()}"
    except Exception as e:
      print(f"Error decoding JSON: {e} (line: {line})")
      raise e
    pts.append(pt)
  ret = pd.DataFrame(pts).replace({np.nan: pd.NA})
  ret = simplify_columns(ret)
  ret = ret.sort_values(by=['N']).reset_index(drop=True)
  return ret.copy()

def load_df():
  P = gather_points_from_files(files)
  return P

mpl.rcParams['axes.autolimit_mode'] = 'round_numbers'

def draw_figure(data: pd.DataFrame, x_name='x', y_name='y', title="", columns='implementation', sort_asc=True, /, **kwargs):
  tbl = data.pivot_table(
    index=x_name,
    columns=columns,
    values=y_name,
    aggfunc='mean',
    sort=False
  ).copy()
  # Sort columns by the value of the highest common point between columns
  common_xs = set.intersection(*[set(tbl.index[~tbl[col].isna()]) for col in tbl.columns])
  col_order = []
  col_values = {}
  for col in tbl.columns:
    vals = tbl.loc[list(common_xs), col]
    avg_val = vals.mean()
    col_values[col] = avg_val
  sorted_names = sorted(col_values.keys(), key=lambda c: col_values[c], reverse=not sort_asc)

  plt.close('all')
  for name in sorted_names:
    col = tbl[name]
    xs = []
    ys = []
    for idx in tbl.index:
      if pd.isna(col.loc[idx]):
        continue
      xs.append(idx)
      ys.append(col.loc[idx])
    plt.plot(xs, ys, label=name)


  for k,v in kwargs.items():
    if v.__class__.__name__ == 'tuple':
      getattr(plt, k)(*v)
    if v.__class__.__name__ == 'dict':
      getattr(plt, k)(**v)
    else:
      getattr(plt, k)(v)

  plt.title(f"{title}")
  filename = title.replace("(", " ").replace(")", " ").replace(",", " ").replace("  ", " ").replace("  ", " ").replace("  ", " ").replace(" ", "_").replace(":", "-").replace("/", "-")
  plt.savefig(f"figures/{filename}.png")

def HEADER(title):
  print(f"### {title}\n")
def SUBHEADER(title):
  print(f"#### {title}\n")
def TITLE(title):
  print(f"##### {title}\n")
def NOTES(note):
  print(f"{note}")
def TEXT(text):
  print(f"{text}")
def NL(n=1):
  print("\n" * (n-1))
def HORIZONTAL_LINE():
  print("\n---\n")

def simp_float(v):
  try:
    ret = f"{float(v):.2f}  "
    return ret
  except:
    return v

def draw_table(data: pd.DataFrame, 
               x_name='x', 
               y_name='y', 
               highlight=-1, # 0 for none, -1 for min, 1 for max
               columns='implementation',
               col_order=[
  "olabs_rostl", "olabs_oram", "olabs_umap", "olabs_umap_shortkv", "Signal", "h2o2", "meta_oram",
]):
  tbl = data.pivot_table(
    index=x_name,
    columns=columns,
    values=y_name,
    aggfunc='mean',
    sort=False
  ).copy()

  if highlight == -1:
    row_min = tbl.min(axis=1, skipna=True)
  else:
    row_min = tbl.max(axis=1, skipna=True)
  tbl = tbl.astype(object)
  for idx in tbl.index:
    rmin = row_min.loc[idx]
    for col in tbl.columns:
      if pd.isna(tbl.loc[idx, col]):
        tbl.loc[idx, col] = ""
        continue
      if highlight != 0:
        if tbl.loc[idx, col] == rmin:
          tbl.loc[idx, col] = f"**{tbl.loc[idx, col]:.2f}**"
          continue
      tbl.loc[idx, col] = simp_float(tbl.loc[idx, col])
  
  if col_order is not None:
    present = []
    used = set()
    for c in col_order:
      cur = []
      for c2 in tbl.columns:
        if c2 in used:
          continue
        if c in c2:
          cur.append(c2)
          used.add(c2)
      cur = sorted(cur)
      for c in cur:
        present.append(c)
    rest = [c for c in tbl.columns if c not in present]
    tbl = tbl.reindex(columns=present + rest)
  
  md = tbl.to_markdown(tablefmt="github", stralign="right", numalign="right", disable_numparse=True, preserve_whitespace=True)
  print(md)
