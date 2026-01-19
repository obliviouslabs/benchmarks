import subprocess
import sys
import tempfile
import psutil
import time
import os
import json
import platform
import socket
import re

def check_if_alread_ran(file_name):
  if os.path.exists(file_name):
    with open(file_name) as f:
      try:
        json.load(f)
        return True
      except:
        pass
  return False

def run_process_registering_memory(cmd, threads=1, timeout=3600):
    if threads < 32:
      target_cmd = f"taskset -c 0-{threads-1} {cmd}"
    else:
      target_cmd = cmd
    
    with tempfile.NamedTemporaryFile(delete=False, mode="w+") as tf:
      time_path = tf.name
    
    target_cmd = f"/usr/bin/time -v {target_cmd}"
    process = subprocess.Popen(
      target_cmd,
      shell=True,
      stdout=open(time_path, "w"),
      stderr=subprocess.STDOUT,
    )

    ret = 0
    max_rss = 0
    max_vss = 0
    outlines = []
    try:
      process.wait(timeout=timeout)
      ret = process.returncode
      outlines = open(time_path).readlines()
      print("".join(outlines))
      os.unlink(time_path)
      m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", "".join(outlines))
      if m:
        max_rss = int(m.group(1)) * 1024
      m = re.search(r"Maximum virtual memory size \(kbytes\):\s*(\d+)", "".join(outlines))
      if m:
        max_vss = int(m.group(1)) * 1024
    except:
      try:
        for _ in range(5):
          process.kill()
      except:
        pass
      pass

    return ret, (max_rss, max_vss), outlines

def parse_mean_time_from_outlines(outlines):
    outlines = [line for line in  outlines if "real_time_mean" in line]
    assert len(outlines) in [0, 1], "Expected only one line with real_time_mean"
    if len(outlines) == 1:
      line = outlines[0]
      t = line.split("real_time_mean")[1].strip().split()[0]
      # line can be an int, a float in decimal notation, or a float in scientific notation (e+{k})
      # we want to turn it into an int number of nanoseconds
      t = float(t)
      return t
    return None

last_time = 0
def report(bench:str , impl: str, mem: tuple[int,int], args, cores=None):
  global last_time
  args_str = "|".join([f" {k} := {v} " for k,v in args.items()])

  sn = platform.system() or ""
  hn = socket.gethostname() or ""
  sn = sn.replace("|", "").replace("=", "")
  hn = hn.replace("|", "").replace("=", "")
  lcores = cores or psutil.cpu_count(logical=True) or 0
  pcores = cores or psutil.cpu_count(logical=False) or 0
  rss = mem[0]
  vss = mem[1]
  vm = psutil.virtual_memory()
  sm = psutil.swap_memory()
  total_mem = vm.total
  total_swap = sm.total
  used_mem = rss
  out = f"{bench}|{impl}| sys_name := {sn} | sys_hostname := {hn} | sys_lcores := {lcores} | sys_pcores := {pcores} | sys_total_mem_bytes := {total_mem} | sys_used_mem_b := {used_mem} | sys_total_swap_b := {total_swap} | sys_used_swap_b := 0 | {args_str}"
  
  print(time.time() - last_time)
  last_time = time.time()
  out = f"\nREPORT_123{out}REPORT_123\n"
  print(out)