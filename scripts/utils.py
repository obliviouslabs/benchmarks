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
        if row is None or row.__class__ == str and row.strip() == "":
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
        if row is None or row.__class__ == str and row.strip() == "":
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
    filename = __file__.split('/')[:-1] + ["..", file]
    filename = '/'.join(filename)
    with open(filename, "r") as f:
      lines += f.readlines()
  
  pts = list()
  for line in lines:
    try:
      if line.startswith("#") or line.strip() == "": continue
      pt = json.loads(line)
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

def get_lines(
  data: pd.DataFrame,
  x_name='x',
  y_name='y',
  columns='implementation',
  sort=-1, # -1 for desc, 1 for asc, 'a' for alphabetical asc, 'd' for alphabetical desc
):
  tbl = data.pivot_table(
    index=x_name,
    columns=columns,
    values=y_name,
    aggfunc='mean',
    sort=None
  ).copy()

  if len(tbl) == 0:
    return []

  ret = []
  for name, col in tbl.items():
    line = {}
    for idx in tbl.index:
      if pd.isna(col.loc[idx]):
        continue
      line[idx] = col.loc[idx]
    ret.append((name, line))

  if sort == 'a':
    ret = sorted(ret, key=lambda x: x[0])
  elif sort == 'd':
    ret = sorted(ret, key=lambda x: x[0], reverse=True)
  elif sort == 1 or sort == -1:
    common_xs = ret[0][1].keys()
    for name, line in ret:
      common_xs = common_xs.intersection(line.keys())
    max_common_x = max(common_xs)
    if sort == 1:
      ret = sorted(ret, key=lambda x: x[1][max_common_x])
    else: # sort ==-1:
      ret = sorted(ret, key=lambda x: x[1][max_common_x], reverse=True)
  
  return ret

def draw_figure(
    data: pd.DataFrame, 
    x_name='x',
    y_name='y',
    title="",
    columns='implementation',
    sort=None,
    /, **kwargs):
  lines = get_lines(
    data,
    x_name=x_name,
    y_name=y_name,
    columns=columns,
    sort=sort
  )

  plt.close('all')
  for name, line in lines:
    xs, ys = line.keys(), line.values()
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
  plt.savefig(f"figures/pdf/{filename}.pdf")
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
        if c in str(c2):
          cur.append(c2)
          used.add(c2)
      cur = sorted(cur)
      for c in cur:
        present.append(c)
    rest = [c for c in tbl.columns if c not in present]
    rest = sorted(rest)
    tbl = tbl.reindex(columns=present + rest)
  
  md = tbl.to_markdown(tablefmt="github", stralign="right", numalign="right", disable_numparse=True, preserve_whitespace=True)
  print(md)


# For Non-Amortized Hierarchical ORAM percentiles, we calculate them the following way:
# avg = value in the latency from the average of a lot of accesses
# We know that for n accesess, n/2 will have cost 0.5, n/4 will have cost 1, n/8 will have cost 2, n/16 will have cost 4...
# so we use this to compute the percentile and multiply by the average to get the estimated percentile latency
def get_percentile_factor(p):
  if p <= 0.5:
    return 0.5
  factor = 0.5
  acc = 0.5
  curr = 0.5
  while acc < p:
    factor *= 2
    curr /= 2
    acc += curr
  return factor
