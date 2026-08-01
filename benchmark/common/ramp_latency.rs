//! Open-loop latency workload shared by the Rust map adapters.
//!
//! A producer publishes requests on an absolute schedule while the calling
//! thread performs synchronous lookups. If a lookup stalls, later arrivals
//! remain queued and their latency includes that queueing delay. Raw records
//! stay in pre-faulted memory until the timed run and queue drain are complete.

use std::cell::UnsafeCell;
use std::env;
use std::fs::File;
use std::hint::spin_loop;
use std::io::{self, BufWriter, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

#[derive(Clone, Debug)]
pub struct Config {
    pub map_size: u64,
    pub min_queries: u64,
    pub max_queries: u64,
    pub phase_queries: u64,
    pub calibration_queries: u64,
    pub overload_phases: u32,
    pub start_fraction: f64,
    pub step_fraction: f64,
    pub implementation: String,
    pub csv_path: PathBuf,
}

impl Config {
    pub fn from_env(
        map_size: u64,
        implementation: impl Into<String>,
        csv_path: impl Into<PathBuf>,
    ) -> Self {
        let min_factor = env_u64("RAMP_MIN_QUERIES_FACTOR", 3);
        let max_factor = env_u64("RAMP_MAX_QUERIES_FACTOR", 8);
        let min_default = map_size.saturating_mul(min_factor);
        let max_default = map_size.saturating_mul(max_factor);
        let min_queries = env_u64("RAMP_MIN_QUERIES", min_default);
        let mut max_queries = env_u64("RAMP_MAX_QUERIES", max_default);
        max_queries = max_queries.max(min_queries);

        let phase_default = (map_size / 8).max(64);
        let calibration_default = map_size.clamp(4096, 16384);

        Self {
            map_size,
            min_queries,
            max_queries,
            phase_queries: env_u64("RAMP_PHASE_QUERIES", phase_default).max(1),
            calibration_queries: env_u64("RAMP_CALIBRATION_QUERIES", calibration_default).max(1),
            overload_phases: env_u64("RAMP_OVERLOAD_PHASES", 2).max(1) as u32,
            start_fraction: positive_env_f64("RAMP_START_FRACTION", 0.25),
            step_fraction: positive_env_f64("RAMP_STEP_FRACTION", 0.025),
            implementation: implementation.into(),
            csv_path: csv_path.into(),
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct Summary {
    pub queries: u64,
    pub elapsed_ns: u64,
    pub calibration_qps: f64,
    pub final_target_qps: f64,
    pub average_latency_ns: f64,
    pub p50_ns: u64,
    pub p95_ns: u64,
    pub p99_ns: u64,
    pub p999_ns: u64,
    pub max_ns: u64,
    pub stop_reason: &'static str,
}

#[derive(Clone, Copy)]
struct Record {
    incoming_ns: u64,
    responded_ns: u64,
}

struct Records(UnsafeCell<Box<[Record]>>);

// The producer writes incoming_ns before publishing the corresponding index.
// The consumer is the sole writer of responded_ns, and output is read only
// after the producer is joined and the consumer has drained the queue.
unsafe impl Sync for Records {}

impl Records {
    fn new(count: usize) -> Self {
        // Filling with non-zero values forces the pages to be physically
        // touched before timing begins.
        Self(UnsafeCell::new(
            vec![
                Record {
                    incoming_ns: u64::MAX,
                    responded_ns: u64::MAX,
                };
                count
            ]
            .into_boxed_slice(),
        ))
    }

    unsafe fn write_incoming(&self, index: usize, value: u64) {
        // SAFETY: each index is written once by the sole producer, before its
        // release-store to produced.
        unsafe {
            (*self.0.get())[index].incoming_ns = value;
        }
    }

    unsafe fn write_responded(&self, index: usize, value: u64) {
        // SAFETY: each response field is written once by the sole consumer.
        unsafe {
            (*self.0.get())[index].responded_ns = value;
        }
    }

    unsafe fn get(&self, index: usize) -> Record {
        // SAFETY: called for published and completed records after queue drain.
        unsafe { (*self.0.get())[index] }
    }

    unsafe fn completed_mut(&self, count: usize) -> &mut [Record] {
        // SAFETY: the caller guarantees that producer and consumer access has
        // ended before taking this unique mutable view.
        unsafe {
            let records: &mut Box<[Record]> = &mut *self.0.get();
            &mut records[..count]
        }
    }
}

struct Shared {
    records: Records,
    start: Instant,
    produced: AtomicU64,
    responded: AtomicU64,
    service_ns: AtomicU64,
    producer_done: AtomicBool,
}

struct ProducerResult {
    queries: u64,
    final_target_qps: f64,
    stop_reason: &'static str,
}

pub fn calibrate<F>(query_count: u64, lookup: &mut F) -> f64
where
    F: FnMut(u64),
{
    // Warm the map before using the same-sized second pass for calibration.
    for query_id in 0..query_count {
        lookup(query_id);
    }
    let start = Instant::now();
    for query_id in 0..query_count {
        lookup(query_id);
    }
    let elapsed_ns = duration_ns(start.elapsed()).max(1);
    query_count as f64 * 1e9 / elapsed_ns as f64
}

pub fn run<F>(config: &Config, calibration_qps: f64, mut lookup: F) -> io::Result<Summary>
where
    F: FnMut(u64),
{
    if config.max_queries == 0 || !calibration_qps.is_finite() || calibration_qps <= 0.0 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "invalid ramp configuration or calibration throughput",
        ));
    }
    let record_count = usize::try_from(config.max_queries).map_err(|_| {
        io::Error::new(
            io::ErrorKind::InvalidInput,
            "maximum query count does not fit in usize",
        )
    })?;

    let shared = Arc::new(Shared {
        records: Records::new(record_count),
        start: Instant::now() + Duration::from_millis(10),
        produced: AtomicU64::new(0),
        responded: AtomicU64::new(0),
        service_ns: AtomicU64::new(0),
        producer_done: AtomicBool::new(false),
    });

    let producer_shared = Arc::clone(&shared);
    let producer_config = config.clone();
    let producer =
        thread::spawn(move || produce(producer_shared, producer_config, calibration_qps));

    let mut query_id = 0_u64;
    let mut local_service_ns = 0_u64;
    loop {
        let produced = shared.produced.load(Ordering::Acquire);
        if query_id < produced {
            let service_start = Instant::now();
            lookup(query_id);
            let response = Instant::now();
            local_service_ns = local_service_ns
                .saturating_add(duration_ns(response.duration_since(service_start)));
            let responded_ns = duration_ns(response.duration_since(shared.start));
            // SAFETY: query_id has been published by the producer; this is the
            // sole consumer and sole writer of this record's response field.
            unsafe {
                shared
                    .records
                    .write_responded(query_id as usize, responded_ns);
            }
            query_id += 1;
            if query_id & 63 == 0 {
                shared.service_ns.store(local_service_ns, Ordering::Release);
                shared.responded.store(query_id, Ordering::Release);
            }
        } else if shared.producer_done.load(Ordering::Acquire) {
            break;
        } else {
            spin_loop();
        }
    }

    shared.service_ns.store(local_service_ns, Ordering::Release);
    shared.responded.store(query_id, Ordering::Release);
    let producer_result = producer
        .join()
        .map_err(|_| io::Error::other("ramp producer thread panicked"))?;

    let summary = write_results(config, calibration_qps, &shared, producer_result)?;
    print_summary(config, &summary);
    Ok(summary)
}

fn produce(shared: Arc<Shared>, config: Config, calibration_qps: f64) -> ProducerResult {
    let mut query_id = 0_u64;
    let mut current_phase = u64::MAX;
    let mut overloaded_checks = 0_u32;
    let mut scheduled_offset_ns = 0_f64;
    let mut target_qps = calibration_qps * config.start_fraction;
    let mut stop_reason = "max_queries";

    while query_id < config.max_queries {
        let phase = query_id / config.phase_queries;
        if phase != current_phase {
            let responded = shared.responded.load(Ordering::Acquire);
            let service_ns = shared.service_ns.load(Ordering::Acquire);
            let backlog = query_id.saturating_sub(responded);
            let service_qps = if service_ns == 0 {
                f64::INFINITY
            } else {
                responded as f64 * 1e9 / service_ns as f64
            };

            target_qps =
                calibration_qps * (config.start_fraction + config.step_fraction * phase as f64);
            current_phase = phase;

            if query_id >= config.min_queries
                && backlog >= config.phase_queries
                && target_qps > service_qps
            {
                overloaded_checks += 1;
            } else {
                overloaded_checks = 0;
            }

            if overloaded_checks >= config.overload_phases {
                stop_reason = "sustained_overload";
                break;
            }
        }

        scheduled_offset_ns += 1e9 / target_qps;
        let scheduled = shared.start + Duration::from_nanos(scheduled_offset_ns as u64);
        wait_until(scheduled);
        let incoming_ns = duration_ns(Instant::now().duration_since(shared.start));
        // SAFETY: this is the sole producer, and it publishes this index with a
        // release-store only after the record has been initialized.
        unsafe {
            shared
                .records
                .write_incoming(query_id as usize, incoming_ns);
        }
        shared.produced.store(query_id + 1, Ordering::Release);
        query_id += 1;
    }

    shared.producer_done.store(true, Ordering::Release);
    ProducerResult {
        queries: query_id,
        final_target_qps: target_qps,
        stop_reason,
    }
}

fn wait_until(target: Instant) {
    const SLEEP_THRESHOLD: Duration = Duration::from_micros(50);
    const SLEEP_MARGIN: Duration = Duration::from_micros(25);
    loop {
        let now = Instant::now();
        if now >= target {
            return;
        }
        let remaining = target.duration_since(now);
        if remaining > SLEEP_THRESHOLD {
            thread::sleep(remaining - SLEEP_MARGIN);
        } else {
            spin_loop();
        }
    }
}

fn write_results(
    config: &Config,
    calibration_qps: f64,
    shared: &Shared,
    producer: ProducerResult,
) -> io::Result<Summary> {
    let count = producer.queries as usize;
    let mut summary = Summary {
        queries: producer.queries,
        calibration_qps,
        final_target_qps: producer.final_target_qps,
        stop_reason: producer.stop_reason,
        ..Summary::default()
    };
    if count != 0 {
        let final_record = unsafe { shared.records.get(count - 1) };
        summary.elapsed_ns = final_record.responded_ns;
    }

    // No file is opened until all timed work and queue draining has finished.
    let mut csv = BufWriter::new(File::create(&config.csv_path)?);
    writeln!(csv, "query_id,incoming_ns,responded_ns")?;
    for index in 0..count {
        let record = unsafe { shared.records.get(index) };
        writeln!(
            csv,
            "{index},{},{}",
            record.incoming_ns, record.responded_ns
        )?;
    }
    csv.flush()?;

    // The raw file is durable now, so reuse the record buffer itself to
    // calculate exact percentiles without allocating a second large array.
    let records = unsafe { shared.records.completed_mut(count) };
    let mut latency_sum = 0_u128;
    for record in records.iter_mut() {
        let latency = record.responded_ns - record.incoming_ns;
        record.responded_ns = latency;
        latency_sum += latency as u128;
    }
    records.sort_unstable_by_key(|record| record.responded_ns);
    if count != 0 {
        summary.average_latency_ns = latency_sum as f64 / count as f64;
        summary.p50_ns = percentile(records, 0.50);
        summary.p95_ns = percentile(records, 0.95);
        summary.p99_ns = percentile(records, 0.99);
        summary.p999_ns = percentile(records, 0.999);
        summary.max_ns = records.last().map_or(0, |record| record.responded_ns);
    }

    let meta_path = metadata_path(&config.csv_path);
    let mut meta = BufWriter::new(File::create(meta_path)?);
    writeln!(meta, "{{")?;
    writeln!(meta, "  \"schema_version\": 1,")?;
    writeln!(
        meta,
        "  \"implementation\": \"{}\",",
        json_escape(&config.implementation)
    )?;
    writeln!(meta, "  \"map_size\": {},", config.map_size)?;
    writeln!(meta, "  \"queries\": {},", summary.queries)?;
    writeln!(meta, "  \"minimum_queries\": {},", config.min_queries)?;
    writeln!(meta, "  \"maximum_queries\": {},", config.max_queries)?;
    writeln!(meta, "  \"phase_queries\": {},", config.phase_queries)?;
    writeln!(
        meta,
        "  \"warmup_queries\": {},",
        config.calibration_queries
    )?;
    writeln!(
        meta,
        "  \"calibration_queries\": {},",
        config.calibration_queries
    )?;
    writeln!(
        meta,
        "  \"calibration_qps\": {:.9},",
        summary.calibration_qps
    )?;
    writeln!(meta, "  \"start_fraction\": {:.9},", config.start_fraction)?;
    writeln!(meta, "  \"step_fraction\": {:.9},", config.step_fraction)?;
    writeln!(
        meta,
        "  \"final_target_qps\": {:.9},",
        summary.final_target_qps
    )?;
    writeln!(meta, "  \"stop_reason\": \"{}\",", summary.stop_reason)?;
    writeln!(
        meta,
        "  \"average_latency_ns\": {:.3},",
        summary.average_latency_ns
    )?;
    writeln!(meta, "  \"p50_latency_ns\": {},", summary.p50_ns)?;
    writeln!(meta, "  \"p95_latency_ns\": {},", summary.p95_ns)?;
    writeln!(meta, "  \"p99_latency_ns\": {},", summary.p99_ns)?;
    writeln!(meta, "  \"p999_latency_ns\": {},", summary.p999_ns)?;
    writeln!(meta, "  \"max_latency_ns\": {}", summary.max_ns)?;
    writeln!(meta, "}}")?;
    meta.flush()?;

    Ok(summary)
}

fn print_summary(config: &Config, summary: &Summary) {
    eprintln!(
        "RAMP_LATENCY implementation={} N={} queries={} stop={} \
         calibration_qps={:.2} final_input_qps={:.2} avg_us={:.3} \
         p50_us={:.3} p95_us={:.3} p99_us={:.3} p999_us={:.3} \
         max_us={:.3} raw={}",
        config.implementation,
        config.map_size,
        summary.queries,
        summary.stop_reason,
        summary.calibration_qps,
        summary.final_target_qps,
        summary.average_latency_ns / 1000.0,
        summary.p50_ns as f64 / 1000.0,
        summary.p95_ns as f64 / 1000.0,
        summary.p99_ns as f64 / 1000.0,
        summary.p999_ns as f64 / 1000.0,
        summary.max_ns as f64 / 1000.0,
        config.csv_path.display(),
    );
}

fn duration_ns(duration: Duration) -> u64 {
    duration.as_nanos().min(u64::MAX as u128) as u64
}

fn percentile(sorted: &[Record], fraction: f64) -> u64 {
    if sorted.is_empty() {
        return 0;
    }
    let index = ((fraction * sorted.len() as f64).ceil() as usize)
        .saturating_sub(1)
        .min(sorted.len() - 1);
    sorted[index].responded_ns
}

fn metadata_path(csv_path: &Path) -> PathBuf {
    let mut value = csv_path.as_os_str().to_os_string();
    value.push(".meta.json");
    PathBuf::from(value)
}

fn env_u64(name: &str, fallback: u64) -> u64 {
    env::var(name)
        .ok()
        .and_then(|value| value.parse().ok())
        .unwrap_or(fallback)
}

fn positive_env_f64(name: &str, fallback: f64) -> f64 {
    env::var(name)
        .ok()
        .and_then(|value| value.parse::<f64>().ok())
        .filter(|value| value.is_finite() && *value > 0.0)
        .unwrap_or(fallback)
}

fn json_escape(value: &str) -> String {
    value
        .replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('\n', "\\n")
        .replace('\r', "\\r")
        .replace('\t', "\\t")
}
