use rand::Rng;
use rostl_oram::{
  circuit_oram::CircuitORAM, prelude::PositionType
};
use rostl_datastructures::array::DynamicArray;
use rostl_datastructures::map::UnsortedMap;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;
use bytemuck::{Zeroable, Pod};
use rostl_datastructures::sharded_map::ShardedMap;
use rostl_primitives::{
  cmov_body, cxchg_body, impl_cmov_for_pod,
  traits::{Cmov, _Cmovbase},
};



fn benchmark_nroram(n: usize) -> i32 {
  const READ_SAMPLES :u64 = 1_000_000;
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  let mut rng = rand::rng();

  let mut nroram = CircuitORAM::<u64>::new(n);
  nroram.write_or_insert(0, 0, 0, 0);
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  for _ in 0..READ_SAMPLES {
    let pos = rng.random_range(0..n as PositionType);
    let mut _value = 0;
    black_box(nroram.read(pos, black_box(0), black_box(0), &mut _value));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);

  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / READ_SAMPLES as f64;

  report_line!(
    "NRORAM",
    "olabs_rostl",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Initialization_zeroed_time_us := {} | Read_latency_us := {} | Read_throughput_qps := {} | Memory_kb := {}",
    n,
    create_time_ns / 1_000,
    avg_ns / 1_000.0,
    (READ_SAMPLES as f64 / (end_query_ns - start_query_ns) as f64) * 1e9,
    mem_diff
  );

  0
}

fn benchmark_roram(n: usize) -> i32 {
  const READ_SAMPLES :u64 = 500_000;
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  let mut rng = rand::rng();

  let mut roram = DynamicArray::<u64>::new(n);  
  roram.write(0, 0);
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  for _ in 0..READ_SAMPLES {
    let pos = rng.random_range(0..n);
    let mut _value = 0;
    black_box(roram.read(black_box(pos), &mut _value));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);

  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / READ_SAMPLES as f64;

  report_line!(
    "RORAM",
    "olabs_rostl",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Initialization_zeroed_time_us := {} | Read_latency_us := {} | Read_throughput_qps := {} | Memory_kb := {}",
    n,
    create_time_ns / 1_000,
    avg_ns / 1_000.0,
    (READ_SAMPLES as f64 / (end_query_ns - start_query_ns) as f64) * 1e9,
    mem_diff
  );

  0
}

#[derive(Default, Clone, Copy, Debug, Zeroable, PartialEq, Eq, PartialOrd, Ord)]
struct B448 {
  a: u64,
  b: u64,
  c: u64,
  d: u64,
  e: u64,
  f: u64,
  g: u64,
}
unsafe impl Pod for B448 {}
impl_cmov_for_pod!(B448);



fn benchmark_umap<VT: Default + Ord + Cmov + Pod + std::fmt::Debug + Send>(n: usize) -> i32 {
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  let mut rng = rand::rng();
  const READ_SAMPLES :u64 = 500_000;

  let mut umap = UnsortedMap::<u64, VT>::new(n);
  umap.insert(0, VT::default());
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  for _ in 0..READ_SAMPLES {
    let mut _value = VT::default();
    black_box(umap.get(black_box(0), black_box(&mut _value)));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);
  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / READ_SAMPLES as f64;
  report_line!(
    "UnorderedMap",
    "olabs_rostl",
    "N := {} | Key_bytes := 8 | Value_bytes := {} | fill := 0.8 | Initialization_zeroed_time_us := {} | Get_latency_us := {} | Get_throughput_qps := {} | Memory_kb := {}",
    n,
    size_of::<VT>(),
    create_time_ns / 1_000,
    avg_ns / 1_000.0,
    (READ_SAMPLES as f64 / (end_query_ns - start_query_ns) as f64) * 1e9,
    mem_diff
  );

  0
}


fn benchmark_sharded_umap<VT: Default + Ord + Cmov + Pod + std::fmt::Debug + Send, const B: usize, const queries_per_batch: usize>(n: usize) -> i32{
  const READ_SAMPLES :usize = 1_000_000;

  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();

  let num_batches = READ_SAMPLES.div_ceil(queries_per_batch) as usize;
  let num_queries = num_batches * queries_per_batch;

  let mut umap = ShardedMap::<u64, VT, B>::new(n);
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  let mut keys = [0; queries_per_batch];

  // let mut rng = rand::rng();
  for i in 0..queries_per_batch {
    keys[i] = i as u64;
  }

  for _ in 0..num_batches {
    let _vals = black_box(umap.get_batch_distinct(black_box(&mut keys)));
  }

  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);
  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_latency_ns = (end_query_ns - start_query_ns) as f64 / num_batches as f64;
  let throughput_qps = (num_queries as f64 / (end_query_ns - start_query_ns) as f64) * 1e9;

  report_line!(
    "UnorderedMap",
    "olabs_rostl_sharded",
    "N := {} | Key_bytes := 8 | Value_bytes := {} | fill := 0.8 | Batch_size := {} | Shards := 15 | Initialization_zeroed_time_us := {} | Get_latency_us := {} | Get_throughput_qps := {} | Memory_kb := {}",
    n,
    size_of::<VT>(),
    queries_per_batch,
    create_time_ns / 1_000,
    avg_latency_ns / 1_000.0,
    throughput_qps,
    mem_diff
  );

  0
}



/// Should take less than 1 hour to run
fn main() {
  // UNDONE(): Should take ___ to run
  for i in 10..=28 {
    let val = 1 << i;    
    let test_name = format!("benchmark_nroram<u64,u64>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_nroram(val)
    });
  }

  // UNDONE(): Should take ___ to run
  for i in 10..=28 {
    let val = 1 << i;
    let test_name = format!("benchmark_roram<u64,u64>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_roram(val)
    });
  }

  // UNDONE(): Should take ___ to run
  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_umap<u64,B448>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_umap::<B448>(val)
    });
  }

  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_umap<u64,u64>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_umap::<u64>(val)
    });
  }

  // UNDONE(): Should take ___ to run
  for i in 10..=28 {
    let val = 1 << i;
    let test_name = format!("benchmark_sharded_umap<u64,B448>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_sharded_umap::<B448, 305, 4096>(val)
    });
  }

  // for i in 10..=24 {
  //   let val = 1 << i;
  //   let test_name = format!("benchmark_sharded_umap<u64,B448>(1<<{})", i);
  //   run_test_forked(&test_name, || {
  //     benchmark_sharded_umap::<90, 1000>(val)
  //   });
  // }


  // for i in 10..=24 {
  //   let val = 1 << i;
  //   let test_name = format!("benchmark_sharded_umap<u64,B448>(1<<{})", i);
  //   run_test_forked(&test_name, || {
  //     benchmark_sharded_umap::<20, 100>(val)
  //   });
  // }


}