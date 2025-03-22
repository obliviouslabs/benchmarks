use rand::Rng;
use rods_oram::{
  circuit_oram::CircuitORAM, prelude::PositionType
};
use rods_datastructures::array::DynamicArray;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;


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




/// Should take less than 1 hour to run
fn main() {
  for i in 10..=26 {
    let val = 1 << i;    
    let test_name = format!("benchmark_nroram<u64,u64>(1<<{})", i);
    // run_test_forked takes the test name and a closure returning i32:
    run_test_forked(&test_name, || {
      // Call your benchmark with the chosen generics
      benchmark_nroram(val)
    });
  }

  for i in 10..=26 {
    let val = 1 << i;
    let test_name = format!("benchmark_roram<u64,u64>(1<<{})", i);
    // run_test_forked takes the test name and a closure returning i32:
    run_test_forked(&test_name, || {
      // Call your benchmark with the chosen generics
      benchmark_roram(val)
    });
  }
}