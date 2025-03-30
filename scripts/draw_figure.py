#!/usr/bin/env python3
import argparse
import json
from collections import defaultdict
import matplotlib.pyplot as plt

ap = argparse.ArgumentParser(description="Automatically generate plots for a given set of params.")
ap.add_argument("-f", "--files", type=str, nargs="+", required=True, help="Files to add.")
# x parameter
ap.add_argument("-x", "--xparam", type=str, required=True, help="X parameter to plot.")
# y parameter
ap.add_argument("-y", "--yparam", type=str, required=True, help="Y parameter to plot.")
# Line name parameter
ap.add_argument("-l", "--linenameparam", type=str, default="implementation", help="Name parameter to plot.")
# Constraints for tests parameters
ap.add_argument("-c", "--constraints", type=str, nargs="+", default=[], help="Constraints for tests.")
# Png file name
ap.add_argument("-o", "--output", type=str, default="figures/", help="Output file directory.") 
# title
ap.add_argument("-t", "--title", type=str, default="Plot", help="Title of the plot.")
# x axis label
ap.add_argument("-xl", "--xlabel", type=str, help="X axis label.")
# y axis label  
ap.add_argument("-yl", "--ylabel", type=str, help="Y axis label.")

class DefaultDict(dict):
  def __missing__(self, key):
    # Set default values here
    return ""

args = ap.parse_args()

lines = []
for file in args.files:
  print(f"Reading {file}")
  with open(file, "r") as f:
    lines += f.readlines()

constraints = []
for constraint in args.constraints:
  if "!=" in constraint:
    key, value = constraint.split("!=")
    constraints.append(("!=", key, value))
  elif "=" in constraint:
    key, value = constraint.split("=")  
    constraints.append(("=", key, value))
  elif constraint.startswith("!"):
    key = constraint[1:]
    value = ""
    constraints.append(("!", key, value))
  elif "@" in constraint:
    key, values = constraint.split("@")
    values = values.split(",")
    constraints.append(("@", key, values))
  else:
    key = constraint
    value = ""
    constraints.append(("", key, value))

pts = defaultdict(list)
for line in lines:
  try:
    if line.startswith("#") or line.strip() == "": continue
    pt = json.loads(line)
  except json.JSONDecodeError as e:
    print(f"Error decoding JSON: {e} (line: {line})")
    continue
  for op, key, value in constraints:
    add = True
    if op in ["!=", "=", "", "@"]:
      if key not in pt: add = False
    elif op == "!":
      if key in pt: add = False
    else:
      assert False, f"Unknown operator {op}"

    if add:  
      if op == "=":
        if pt[key] != value:
          add = False
      elif op == "!=":    
        if pt[key] == value:
          add = False
      elif op == "@":
        if pt[key] not in value:
          add = False

    if not add:
      print(f"Skipping {pt} because {key} != {value}")
      break
  else:
    print(f"Adding {pt}")
    try:
      x = pt[args.xparam]
      y = pt[args.yparam]
      name = args.linenameparam.format_map(DefaultDict(pt))
      x = float(x)
      y = float(y)
      pts[name].append((x, y))
    except Exception as e:
      print(f"Exception: {e}")
      continue

# Build a mapping from line name to dict of x -> y for legend sorting
name_to_xy = {name: dict(points) for name, points in pts.items()}

# Find common x values across all line groups
all_x_sets = [set(xy.keys()) for xy in name_to_xy.values()]
if all_x_sets:
    common_xs = set.intersection(*all_x_sets)
else:
    common_xs = set()

if not common_xs:
    print("Warning: No common x values between lines. Using unsorted legend.")
    sorted_names = list(pts.keys())
else:
    # Use the largest common x for sorting
    x_common = max(common_xs)
    sorted_names = sorted(name_to_xy.keys(), key=lambda name: name_to_xy[name][x_common], reverse=True)


for name in sorted_names:
  points = pts[name]
  print(points)
  points = sorted(points)
  x_values = [point[0] for point in points]
  if len([x for x in x_values if x_values.count(x) > 1]) > 0:
    print(f"Warning: {name} has repeated x values: {[x for x in x_values if x_values.count(x) > 1]}")
  xs, ys = zip(*points)
  plt.plot(xs, ys, label=name)

if args.xlabel:
  plt.xlabel(args.xlabel)
else:
  plt.xlabel(args.xparam) 
if args.ylabel:
  plt.ylabel(args.ylabel) 
else:  
  plt.ylabel(args.yparam)
plt.title(f"{args.title}")
plt.legend()
plt.grid(True)
# plt.tight_layout()
plt.xscale("log")
plt.yscale("log")
filename = f'{args.title.replace(" ", "_").replace(":", "-").replace("/", "-")}'
plt.savefig(f"{args.output}/{filename}.png")

