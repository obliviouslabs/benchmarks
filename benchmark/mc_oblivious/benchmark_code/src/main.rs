use aligned_cmov::{typenum, A8Bytes};
use criterion::{black_box, criterion_group, criterion_main, Criterion};
use mc_oblivious_map::{CuckooHashTable, CuckooHashTableCreator};
use mc_oblivious_ram::PathORAM4096Z4Creator;
use mc_oblivious_traits::{HeapORAMStorageCreator, OMapCreator, ORAMCreator, ObliviousHashMap};
use mc_rand::McRng;
use std::time::Duration;
use test_helper::a8_8;
use typenum::{U1024, U32};

pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};

type ORAMCreatorZ4 = PathORAM4096Z4Creator<McRng, HeapORAMStorageCreator>;
type PathORAMZ4 = <ORAMCreatorZ4 as ORAMCreator<U1024, McRng>>::Output;
type Table = CuckooHashTable<U32, U32, U1024, McRng, PathORAMZ4>;
type CuckooCreatorZ4 = CuckooHashTableCreator<U1024, McRng, ORAMCreatorZ4>;

fn make_omap(capacity: u64) -> Table {
    CuckooCreatorZ4::create(capacity, 32, || McRng {})
}



fn benchmark_umap(n: u64) -> i32 {
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  // let mut rng = rand::rng();
  let READ_SAMPLES = 100_000;
  let mut umap = make_omap(n*5/4);

  {
    let key: A8Bytes<U32> = a8_8(1);
    let val: A8Bytes<U32> = a8_8(1);
    umap.vartime_write(&key, &val, 1.into());
  }
  
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();

  for _ in 0..READ_SAMPLES {
    let key: A8Bytes<U32> = a8_8(1);
    let mut _val: A8Bytes<U32> = a8_8(0);
    black_box(umap.read(black_box(&key), black_box(&mut _val)));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);
  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / READ_SAMPLES as f64;
  report_line!(
    "UnorderedMap",
    "mc_oblivious",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | fill := 0.8 | Initialization_zeroed_time_us := {} | Get_latency_us := {} | Get_throughput_qps := {} | Memory_kb := {}",
    n,
    create_time_ns / 1_000,
    avg_ns / 1_000.0,
    (READ_SAMPLES as f64 / (end_query_ns - start_query_ns) as f64) * 1e9,
    mem_diff
  );

  0
}



fn main() {
  // Should take 7 mins to run
  for i in 10..=28 {
    let val = 1 << i;
    let test_name = format!("benchmark_umap<u64,u64>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_umap(val)
    });
  }

  // UNDONE(): Implement roram benchmark
  // for i in 10..=26 {
  //   let val = 1 << i;
  //   let test_name = format!("benchmark_roram<u64,u64>(1<<{})", i);
  //   run_test_forked(&test_name, || {
  //     benchmark_roram(val)
  //   });
  // }
}