#!/usr/bin/env python3
import argparse
import json
from collections import defaultdict
from parse_util import add_args_params, gather_points_from_files
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

args = ap.parse_args()

pts, sorted_names, _ = gather_points_from_files(args.files, args)

for name in sorted_names:
  points = pts[name]
  print(points)
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
filename = f'{args.title.replace("(", " ").replace(")", " ").replace(",", " ").replace("  ", " ").replace("  ", " ").replace("  ", " ").replace(" ", "_").replace(":", "-").replace("/", "-")}'
plt.savefig(f"{args.output}/{filename}.png")

