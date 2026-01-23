from time import time
from utils import parse_mean_time_from_outlines, run_process_registering_memory, check_if_alread_ran, report
import sys

target_dir = sys.argv[1] 
block_sizes = [16, 64]
thread_nums = [1, 32]

for threads in thread_nums:
  for b in block_sizes:
    n_base_max = 28
    if threads == 1:
      n_base_max = 24
    for n_base in range(10, n_base_max + 1):
      n = 2**n_base 
      repetitions = 3 if threads <= 4 else 5
      file_name = f"{target_dir}/results/results_omap_{n}_{b}_{threads}.json"
      if check_if_alread_ran(file_name):
        assert False, "This experiment should not have been run already"

      start_time = time()
      ret, mem, outlines = run_process_registering_memory(f"../bin/ORAMBenchmark  --benchmark_out={file_name} --benchmark_filter=OMapDataFixture{b}/OMap/{n}/* --benchmark_repetitions={repetitions}", threads)
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
      res["Value_bytes"] = b-8
      res["Initialization_time_us"] = t / 1000
      res["Get_latency_us"] = t / n / 1000
      res["Get_max_latency_us"] = t / 1000
      res["Get_throughput_qps"] = n * 1_000_000_000 / t
      res["Memory_kb"] = mem[0] // 1024
      res["Test_time_s"] = end_time - start_time

      report("UnorderedMap", f"h2o2", mem, res, cores=threads)

      print(f"({threads}, {b}, {n}) Process exited with code {ret}, max memory usage: {mem} bytes")