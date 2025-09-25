#!/usr/bin/env -S uv run python3
# /// script
# requires-python = ">=3.8"
# dependencies = [
#   "matplotlib",
# ]
# ///
import argparse
import json
from collections import defaultdict
from parse_util import add_args_params, gather_points_from_files, DefaultDict
import matplotlib.pyplot as plt

ap = argparse.ArgumentParser(description="Automatically generate plots for a given set of params.")
ap.add_argument("-f", "--files", type=str, nargs="+", required=True, help="Files to add.")
add_args_params(ap)

# Png file name
ap.add_argument("-o", "--output", type=str, default="figures/", help="Output file directory.") 
# title
ap.add_argument("-t", "--title", type=str, default="Plot", help="Title of the plot.")
# x axis label
ap.add_argument("-xl", "--xlabel", type=str, help="X axis label.")
# y axis label  
ap.add_argument("-yl", "--ylabel", type=str, help="Y axis label.")
ap.add_argument("-xt", '--xtransform', type=str, default="x", help="How to represent x parameter (via eval)")
ap.add_argument("-yt", '--ytransform', type=str, default='f"{y:.2f}"', help="How to represent y parameter (via eval)")

args = ap.parse_args()

pts, sorted_names, all_xs = gather_points_from_files(args.files, args)

print(pts)
columns = []
x_to_index = {x: i for i, x in enumerate(all_xs)}
for name in sorted_names:
  column = ([""] * len(all_xs))
  for x, y in pts[name]:
    if x in x_to_index:
      column[x_to_index[x]] = eval(args.ytransform.format_map(DefaultDict({"x": x, "y": y})))
  columns.append(column)

# Print the title of the table
print(f"# {args.title}")
print(f" {args.ylabel} ")
print("")

# Print the table in markdown format
print(f"| {args.xlabel} | " + " | ".join(sorted_names) + " |")

print("| :--- | " + " :---: |" * (len(sorted_names)))
for i, x in enumerate(all_xs):
  x_transformed = eval(args.xtransform.format_map(DefaultDict({"x": x})))
  print(f"| ${x_transformed}$ | " + " | ".join(str(columns[j][i]) for j in range(len(columns))) + " |")

