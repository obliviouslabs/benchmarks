use bytemuck::Pod;
use rostl_datastructures::map::UnsortedMap;
use rostl_primitives::traits::Cmov;
use std::env;
use std::hint::black_box;
use std::path::PathBuf;

#[path = "../ramp_latency.rs"]
mod ramp_latency;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let n = env::args()
        .nth(1)
        .map(|value| value.parse::<usize>())
        .transpose()?
        .unwrap_or(1024);
    if n == 0 {
        return Err("map size must be greater than zero".into());
    }
    let output = env::args()
        .nth(2)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(format!("ramp_latency_olabs_rostl_N{n}.csv")));

    run::<u64>(n, output)
}

fn run<V>(n: usize, output: PathBuf) -> Result<(), Box<dyn std::error::Error>>
where
    V: Default + Ord + Cmov + Pod + std::fmt::Debug + Send,
{
    let mut map = UnsortedMap::<u64, V>::new(n);
    map.insert(0, V::default());
    let config = ramp_latency::Config::from_env(n as u64, "olabs_rostl", output);
    let mut lookup = |_query_id| {
        let mut value = V::default();
        black_box(map.get(black_box(0), black_box(&mut value)));
    };
    let calibration_qps = ramp_latency::calibrate(config.calibration_queries, &mut lookup);
    ramp_latency::run(&config, calibration_qps, lookup)?;
    Ok(())
}
