import math
import sys
from time import time
from utils import parse_mean_time_from_outlines, report, run_process_registering_memory, check_if_alread_ran

target_dir = sys.argv[1]
block_sizes = [64]
# block_sizes = [48, 64]
# thread_nums = [1, 32]
thread_nums = [32]

for threads in thread_nums:
  for b in block_sizes:
    cnt_ub = 28
    if threads == 1:
      cnt_ub = 24
    for n_base in range(10, cnt_ub+1):
      repetitions = 1
      n = 2**n_base 
      file_name = f"{target_dir}/results/results_{n}_{b}_{threads}.json"
      if check_if_alread_ran(file_name):
        assert False, "This experiment should not have been run already"

      start_time = time()
      cmd = f"../bin/ORAMBenchmark  --benchmark_out={file_name} --benchmark_filter=ORAMDataFixture{b}/ORAM/{n}/process_time/* --benchmark_repetitions={repetitions}"
      
      ret, mem, outlines = run_process_registering_memory(cmd, threads=threads)
      t = parse_mean_time_from_outlines(outlines)
      if t is None:
        print(f"({threads}, {b}, {n}) Could not parse time from output")
        if n_base > 20:
          # N' > N ==> fail(N) ==> fail(N')
          break
        continue

      end_time = time()

      res = {}
      if threads > 1:
        res["Threads"] = threads
      res["N"] = n
      res["Key_bytes"] = 8
      res["Value_bytes"] = b - 8
      res["Read_latency_us"] = t / n / 1000
      res["Read_max_latency_us"] = t / 2 / 1000
      res["Read_throughput_qps"] = n * 1_000_000_000 / t 
      res["Memory_kb"] = mem[0] // 1024
      res["Test_time_s"] = end_time - start_time

      report("RORAM", f"h2o2", mem, res, cores=threads)

      print(f"({threads}, {b}, {n}) Process exited with code {ret}, max memory usage: {mem} bytes")
