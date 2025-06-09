use rand::Rng;
use rods_oram::{
  circuit_oram::CircuitORAM, prelude::PositionType
};
use rods_datastructures::array::DynamicArray;
use rods_datastructures::map::UnsortedMap;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;
use bytemuck::{Zeroable, Pod};
use rods_datastructures::sharded_map::ShardedMap;
use rods_primitives::{
  cmov_body, cxchg_body, impl_cmov_for_pod,
  traits::{Cmov, _Cmovbase},
};



fn benchmark_nroram(n: usize) -> i32 {
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  let mut rng = rand::rng();

  let mut nroram = CircuitORAM::<u64>::new(n);
  nroram.write_or_insert(0, 0, 0, 0);
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  for _ in 0..n {
    let pos = rng.random_range(0..n as PositionType);
    let mut _value = 0;
    black_box(nroram.read(pos, black_box(0), black_box(0), &mut _value));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);

  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / n as f64;

  report_line!(
    "NRORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Initialization_zeroed_time_us := {}",
    n,
    create_time_ns / 1_000
  );

  report_line!(
    "NRORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Read_latency_us := {}",
    n,
    avg_ns / 1_000.0
  );

  report_line!(
    "NRORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Read_throughput_qps := {}",
    n,
    (n as f64 / (end_query_ns - start_query_ns) as f64) * 1e9
  );

  report_line!(
    "NRORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Memory_kb := {}",
    n,
    mem_diff
  );

  0
}

fn benchmark_roram(n: usize) -> i32 {
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  let mut rng = rand::rng();

  let mut roram = DynamicArray::<u64>::new(n);  
  roram.write(0, 0);
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  for _ in 0..n {
    let pos = rng.random_range(0..n);
    let mut _value = 0;
    black_box(roram.read(black_box(pos), &mut _value));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);

  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / n as f64;

  report_line!(
    "RORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Initialization_zeroed_time_us := {}",
    n,
    create_time_ns / 1_000
  );

  report_line!(
    "RORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Read_latency_us := {}",
    n,
    avg_ns / 1_000.0
  );

  report_line!(
    "RORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Read_throughput_qps := {}",
    n,
    (n as f64 / (end_query_ns - start_query_ns) as f64) * 1e9
  );

  report_line!(
    "RORAM",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 8 | Memory_kb := {}",
    n,
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



fn benchmark_umap(n: usize) -> i32 {
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();
  let mut rng = rand::rng();

  let mut umap = UnsortedMap::<u64, B448>::new(n*5/4);
  umap.insert(0, B448 { a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0 });
  let end_create_ns = current_time_ns();

  let start_query_ns = current_time_ns();
  for _ in 0..n {
    let mut _value = B448 { a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0 };
    black_box(umap.get(black_box(0), black_box(&mut _value)));
  }
  let end_query_ns = current_time_ns();
  let mem_end = get_mem_value().unwrap_or(0);
  let mem_diff = mem_end - mem_start;
  let create_time_ns = end_create_ns - start_create_ns;
  let avg_ns = (end_query_ns - start_query_ns) as f64 / n as f64;
  report_line!(
    "UnorderedMap",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Initialization_zeroed_time_us := {}",
    n,
    create_time_ns / 1_000
  );
  report_line!(
    "UnorderedMap",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Get_latency_us := {}",
    n,
    avg_ns / 1_000.0
  );
  report_line!(
    "UnorderedMap",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Get_throughput_qps := {}",
    n,
    (n as f64 / (end_query_ns - start_query_ns) as f64) * 1e9
  );
  report_line!(
    "UnorderedMap",
    "olabs_rods",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Memory_kb := {}",
    n,
    mem_diff
  );
  0
}


fn benchmark_sharded_umap<const B: usize, const queries_per_batch: usize>(n: usize) -> i32{
  let mem_start = get_mem_value().unwrap_or(0);
  let start_create_ns = current_time_ns();

  let num_batches = n.div_ceil(queries_per_batch) as usize;
  let num_queries = num_batches * queries_per_batch;

  let mut umap = ShardedMap::<u64, B448, B>::new(n*5/4);
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
    "olabs_rods_sharded",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Batch_size := {} | Shards := 15 | Initialization_zeroed_time_us := {}",
    n,
    queries_per_batch,
    create_time_ns / 1_000
  );
  report_line!(
    "UnorderedMap",
    "olabs_rods_sharded",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Batch_size := {} | Shards := 15 | Get_latency_us := {}",
    n,
    queries_per_batch,
    avg_latency_ns / 1_000.0
  );
  report_line!(
    "UnorderedMap",
    "olabs_rods_sharded",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Batch_size := {} | Shards := 15 | Get_throughput_qps := {}",
    n,
    queries_per_batch,
    throughput_qps
  );
  report_line!(
    "UnorderedMap",
    "olabs_rods_sharded",
    "N := {} | Key_bytes := 8 | Value_bytes := 56 | fill := 0.8 | Batch_size := {} | Shards := 15 | Memory_kb := {}",
    n,
    queries_per_batch,
    mem_diff
  );
  0
}



/// Should take less than 1 hour to run
fn main() {
  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_sharded_umap<u64,B448>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_sharded_umap::<305, 4096>(val)
    });
  }

  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_sharded_umap<u64,B448>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_sharded_umap::<90, 1000>(val)
    });
  }


  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_sharded_umap<u64,B448>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_sharded_umap::<20, 100>(val)
    });
  }


  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_umap<u64,B448>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_umap(val)
    });
  }

  for i in 10..=26 {
    let val = 1 << i;    
    let test_name = format!("benchmark_nroram<u64,u64>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_nroram(val)
    });
  }
  
  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_roram<u64,u64>(1<<{})", i);
    run_test_forked(&test_name, || {
      benchmark_roram(val)
    });
  }
}