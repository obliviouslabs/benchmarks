use oram::{DefaultOram, BlockValue, Address, Oram};
use rand::{SeedableRng, Rng};
use rand::rngs::StdRng;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;

fn benchmark_default_oram<const V: usize>(capacity: usize) -> i32 {
    let mem_start = get_mem_value().unwrap_or(0);
    let start_create_ns = current_time_ns();

    const READ_SAMPLES : u64 = 100_000;

    let mut rng = StdRng::seed_from_u64(0);
    let mut oram = DefaultOram::<BlockValue<V>>::new(capacity as Address, &mut rng).unwrap();

    oram.write(0, BlockValue::default(), &mut rng).unwrap();

    let end_create_ns = current_time_ns();

    let start_query_ns = current_time_ns();
    for _ in 0..READ_SAMPLES {
        let pos = rng.gen_range(0..(capacity as Address));
        let _ = black_box(oram.read(pos, &mut rng).unwrap());
    }
    let end_query_ns = current_time_ns();
    let mem_end = get_mem_value().unwrap_or(0);

    let mem_diff = mem_end - mem_start;
    let create_time_ns = end_create_ns - start_create_ns;
    let avg_ns = (end_query_ns - start_query_ns) as f64 / READ_SAMPLES as f64;

    report_line!(
        "RORAM",
        "meta_oram",
        "N := {} | Key_bytes := 8 | Value_bytes := {} | Initialization_zeroed_time_us := {}",
        capacity,
        V,
        create_time_ns / 1_000
    );

    report_line!(
        "RORAM",
        "meta_oram",
        "N := {} | Key_bytes := 8 | Value_bytes := {} | Read_latency_us := {}",
        capacity,
        V,
        avg_ns / 1_000.0
    );

    report_line!(
        "RORAM",
        "meta_oram",
        "N := {} | Key_bytes := 8 | Value_bytes := {} | Read_throughput_qps := {}",
        capacity,
        V,
        (READ_SAMPLES as f64 / (end_query_ns - start_query_ns) as f64) * 1e9
    );

    report_line!(
        "RORAM",
        "meta_oram",
        "N := {} | Key_bytes := 8 | Value_bytes := {} | Memory_kb := {}",
        capacity,
        V,
        mem_diff
    );

    0
}


/// Should take 7h to run
fn main() {
    // Should take 2h20min to run
    // 8b key, 8b value
    for i in 10..=26 {
        let val = 1 << i;
        let test_name = format!("benchmark_meta_oram<BlockValue<8>>(1<<{})", i);
        run_test_forked(&test_name, || {
            benchmark_default_oram::<8>(val)
        });
    }

    // Should take 2h20min to run
    // 8b key, 32b value
    for i in 10..=26 {
        let val = 1 << i;
        let test_name = format!("benchmark_meta_oram<BlockValue<32>>(1<<{})", i);
        run_test_forked(&test_name, || {
            benchmark_default_oram::<32>(val)
        });
    }

    // Should take 2h20min to run
    // 8b key, 56b value
    for i in 10..=26 {
        let val = 1 << i;
        let test_name = format!("benchmark_meta_oram<BlockValue<56>>(1<<{})", i);
        run_test_forked(&test_name, || {
            benchmark_default_oram::<56>(val)
        });
    }
    
  }
