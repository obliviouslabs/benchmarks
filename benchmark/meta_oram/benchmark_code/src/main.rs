use oram::{DefaultOram, BlockValue, Address, Oram};
use rand::{SeedableRng, Rng};
use rand::rngs::StdRng;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;

fn benchmark_default_oram(capacity: usize) -> i32 {
    let mem_start = get_mem_value().unwrap_or(0);
    let start_create_ns = current_time_ns();

    let mut rng = StdRng::seed_from_u64(0);
    let mut oram = DefaultOram::<BlockValue<8>>::new(capacity as Address, &mut rng).unwrap();

    oram.write(0, BlockValue::default(), &mut rng).unwrap();

    let end_create_ns = current_time_ns();

    let start_query_ns = current_time_ns();
    for _ in 0..capacity {
        let pos = rng.gen_range(0..(capacity as Address));
        let _ = black_box(oram.read(pos, &mut rng).unwrap());
    }
    let end_query_ns = current_time_ns();
    let mem_end = get_mem_value().unwrap_or(0);

    let mem_diff = mem_end - mem_start;
    let create_time_ns = end_create_ns - start_create_ns;
    let avg_ns = (end_query_ns - start_query_ns) as f64 / capacity as f64;

    report_line!(
        "DefaultOram",
        "olabs_rods",
        "N := {} | Block_bytes := 8 | Initialization_time_us := {}",
        capacity,
        create_time_ns / 1_000
    );

    report_line!(
        "DefaultOram",
        "olabs_rods",
        "N := {} | Block_bytes := 8 | Read_latency_us := {}",
        capacity,
        avg_ns / 1_000.0
    );

    report_line!(
        "DefaultOram",
        "olabs_rods",
        "N := {} | Block_bytes := 8 | Read_throughput_qps := {}",
        capacity,
        (capacity as f64 / (end_query_ns - start_query_ns) as f64) * 1e9
    );

    report_line!(
        "DefaultOram",
        "olabs_rods",
        "N := {} | Block_bytes := 8 | Memory_kb := {}",
        capacity,
        mem_diff
    );

    0
}

fn main() {
    // for i in 10..=26 {
    //   let val = 1 << i;    
    //   let test_name = format!("benchmark_nroram<u64,u64>(1<<{})", i);
    //   // run_test_forked takes the test name and a closure returning i32:
    //   run_test_forked(&test_name, || {
    //     // Call your benchmark with the chosen generics
    //     benchmark_nroram(val)
    //   });
    // }
  
    // for i in 10..=26 {
    //   let val = 1 << i;
    //   let test_name = format!("benchmark_roram<u64,u64>(1<<{})", i);
    //   // run_test_forked takes the test name and a closure returning i32:
    //   run_test_forked(&test_name, || {
    //     // Call your benchmark with the chosen generics
    //     benchmark_roram(val)
    //   });
    // }

    for i in 10..=26 {
        let val = 1 << i;
        let test_name = format!("benchmark_meta_oram<BlockValue<8>>(1<<{})", i);
        run_test_forked(&test_name, || {
            benchmark_default_oram(val)
        });
    }
    
  }
