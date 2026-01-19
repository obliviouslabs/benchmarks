#!/usr/bin/env python3
'''
Script to parse log files and extract benchmark reports.
Each line containing 'REPORT_123 x | y | z | ... REPORT_123' is parsed to extract x, y, z.
Outputs the extracted data in JSON format.
'''

import re
import argparse
import json

parser = argparse.ArgumentParser(description="Find and extract x, y, z from lines containing 'REPORT_123 x | y | z | ...  REPORT_123'.")
parser.add_argument('-f', '--file', required=True, help='File to search.')
args = parser.parse_args()


def parse_split(split):
  split = split.strip()
  split = split.split(":=")
  assert len(split) in [1, 2]
  for i in range(len(split)):
    split[i] = split[i].strip()
  return split

def parse_line(line):
  reports = line.split("REPORT_123")
  if len(reports) != 3:
    return None

  cleaned = reports[1].strip()
  split = cleaned.split("|")
  for i in range(len(split)):
    split[i] = parse_split(split[i])

  res = {
    "benchmark_type": split[0][0],
    "implementation": split[1][0],
    "extra": []
  }

  for x in split[2:]:
    if len(x) == 1:
      res["extra"].append(x[0])
    else:
      res[x[0]] = x[1]
  
  return res


with open(args.file, "r") as file:
  lines = file.readlines()

for line in lines:
  x = parse_line(line)
  if x is not None:
    print(json.dumps(x))
  