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


args = ap.parse_args()

lines = []
for file in args.files:
  print(f"Reading {file}")
  with open(file, "r") as f:
    lines += f.readlines()

constraints = []
for constraint in args.constraints:
  key, value = constraint.split("=")
  constraints.append((key, value))

pts = defaultdict(list)
for line in lines:
  pt = json.loads(line)
  for key, value in constraints:
    if key not in pt or pt[key] != value:
      print(f"Skipping {pt} because {key} != {value}")
      break
  else:
    print(f"Adding {pt}")
    try:
      x = pt[args.xparam]
      y = pt[args.yparam]
      name = args.linenameparam.format_map(pt)
      x = float(x)
      y = float(y)
      pts[name].append((x, y))
    except KeyError:
      continue

for name, points in pts.items():
  print(points)
  points = sorted(points)
  x_values = [point[0] for point in points]
  if len([x for x in x_values if x_values.count(x) > 1]) > 0:
    print(f"Warning: {name} has repeated x values: {[x for x in x_values if x_values.count(x) > 1]}")
  xs, ys = zip(*points)
  plt.plot(xs, ys, label=name)

plt.xlabel(args.xparam)
plt.ylabel(args.yparam)
plt.title(f"{args.title}")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.xscale("log")
plt.yscale("log")
plt.savefig(f"{args.output}/{args.title}_{args.yparam}.png")

