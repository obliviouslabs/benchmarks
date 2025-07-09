from collections import defaultdict
import argparse
import json

class DefaultDict(dict):
  def __missing__(self, key):
    # Set default values here
    return " "

def parse_constraints(constraints_args):
  constraints = []
  for constraint in constraints_args:
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
  return constraints

def check_constraints(pt, constraints):
  for op, key, value in constraints:
    if op in ["!=", "=", "", "@"]:
      if key not in pt:
        # print(f"Skipping {pt} because {key} doesnt exist")
        return False
    elif op == "!":
      if key in pt:
        # print(f"Skipping {pt} because {key} exists")
        return False
    else:
      assert False, f"Unknown operator {op}"

    if op == "=":
      if pt[key] != value:
        # print(f"Skipping {pt} because {key} != {value}")
        return False
    elif op == "!=":    
      if pt[key] == value:
        # print(f"Skipping {pt} because {key} == {value}")
        return False
    elif op == "@":
      if pt[key] not in value:
        # print(f"Skipping {pt} because {key} not in {value}")
        return False

  return True

def transform_line_name(pt, xparam, yparam, linenameparam):
  x = eval(xparam.format_map(DefaultDict(pt)))
  y = eval(yparam.format_map(DefaultDict(pt)))
  name = linenameparam.format_map(DefaultDict(pt))
  return name, x, y


def add_args_params(ap):
  # x parameter
  ap.add_argument("-x", "--xparam", type=str, required=True, help="X parameter to plot.")
  # y parameter
  ap.add_argument("-y", "--yparam", type=str, required=True, help="Y parameter to plot.")
  # Line name parameter
  ap.add_argument("-l", "--linenameparam", type=str, default="implementation", help="Name parameter to plot.")
  # Constraints for tests parameters
  ap.add_argument("-c", "--constraints", type=str, nargs="+", default=[], help="Constraints for tests.")
  ap.add_argument("-fn", "--filter_names", type=str, nargs="+", default=[], help="Filter names for tests.")
  ap.add_argument("-fo", "--force_order", action="store_true", help="Force order of lines.")



def gather_points_from_files(files, args):
  lines = []
  for file in files:
    print(f"Reading {file}")
    with open(file, "r") as f:
      lines += f.readlines()

  return gather_points_from_lines(lines, args)

def gather_points_from_lines(lines, args):
  constraints = parse_constraints(args.constraints)

  pts = defaultdict(list)
  for line in lines:
    try:
      if line.startswith("#") or line.strip() == "": continue
      pt = json.loads(line)
    except json.JSONDecodeError as e:
      print(f"Error decoding JSON: {e} (line: {line})")
      continue
    if check_constraints(pt, constraints):
      # print(f"Adding {pt}")
      try:
        name, x, y = transform_line_name(pt, args.xparam, args.yparam, args.linenameparam)
        pts[name].append((x, y))
      except Exception as e:
        print(f"Exception: {e}")
        continue

  name_to_xy = {name: dict(points) for name, points in pts.items()}

  if args.filter_names:
    pts = {name: points for name, points in pts.items() if name in args.filter_names}
  
  # Find common x values across all line groups
  all_x_sets = [set(xy.keys()) for xy in name_to_xy.values()]
  if all_x_sets:
    common_xs = set.intersection(*all_x_sets)
    all_xs = sorted(list(set.union(*all_x_sets)), key=float)
  else:
    common_xs = set()
    all_xs = []

  if args.force_order:
    if args.filter_names:
      sorted_names = [x for x in args.filter_names if x in name_to_xy.keys()]
    else:
      sorted_names = list(pts.keys())
  elif not common_xs:
    print("Warning: No common x values between lines. Using unsorted legend.")
    sorted_names = list(pts.keys())
  else:
    # Use the largest common x for sorting
    x_common = max(common_xs)
    sorted_names = sorted(name_to_xy.keys(), key=lambda name: name_to_xy[name][x_common], reverse=True)

  for name in sorted_names:
    points = pts[name]
    pts[name] = sorted(points, key=lambda p: float(p[0]))
    x_values = [point[0] for point in points]
    if len([x for x in x_values if x_values.count(x) > 1]) > 0:
      print(f"Warning: {name} has repeated x values: {[x for x in x_values if x_values.count(x) > 1]}")

  return pts, sorted_names, all_xs