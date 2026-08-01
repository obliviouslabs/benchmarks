use aligned_cmov::{typenum, A8Bytes};
use mc_oblivious_map::{CuckooHashTable, CuckooHashTableCreator};
use mc_oblivious_ram::PathORAM4096Z4Creator;
use mc_oblivious_traits::{HeapORAMStorageCreator, OMapCreator, ORAMCreator, ObliviousHashMap};
use mc_rand::McRng;
use std::env;
use std::hint::black_box;
use std::path::PathBuf;
use test_helper::a8_8;
use typenum::{U1024, U32};

#[path = "../ramp_latency.rs"]
mod ramp_latency;

type ORAMCreatorZ4 = PathORAM4096Z4Creator<McRng, HeapORAMStorageCreator>;
type PathORAMZ4 = <ORAMCreatorZ4 as ORAMCreator<U1024, McRng>>::Output;
type Table = CuckooHashTable<U32, U32, U1024, McRng, PathORAMZ4>;
type CuckooCreatorZ4 = CuckooHashTableCreator<U1024, McRng, ORAMCreatorZ4>;

fn make_map(capacity: u64) -> Table {
    CuckooCreatorZ4::create(capacity, 32, || McRng {})
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let n = env::args()
        .nth(1)
        .map(|value| value.parse::<u64>())
        .transpose()?
        .unwrap_or(1024);
    if n == 0 {
        return Err("map size must be greater than zero".into());
    }
    let output = env::args()
        .nth(2)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(format!("ramp_latency_mc_oblivious_N{n}.csv")));

    let mut map = make_map(n * 5 / 4);
    let key: A8Bytes<U32> = a8_8(1);
    let value: A8Bytes<U32> = a8_8(1);
    map.vartime_write(&key, &value, 1.into());

    let config = ramp_latency::Config::from_env(n, "mc_oblivious", output);
    let mut lookup = |_query_id| {
        let key: A8Bytes<U32> = a8_8(1);
        let mut value: A8Bytes<U32> = a8_8(0);
        black_box(map.read(black_box(&key), black_box(&mut value)));
    };
    let calibration_qps = ramp_latency::calibrate(config.calibration_queries, &mut lookup);
    ramp_latency::run(&config, calibration_qps, lookup)?;
    Ok(())
}
