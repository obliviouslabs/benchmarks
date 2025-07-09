use std::fs::File;
use std::io::{BufRead, BufReader};
use std::process;

use libc::{clock_gettime, timespec, CLOCK_MONOTONIC};

/// Returns a monotonic time in nanoseconds (similar to C's clock_gettime(CLOCK_MONOTONIC,...)).
pub fn current_time_ns() -> u64 {
    let mut ts: timespec = unsafe { std::mem::zeroed() };
    unsafe {
        clock_gettime(CLOCK_MONOTONIC, &mut ts);
    }
    (ts.tv_sec as u64) * 1_000_000_000 + (ts.tv_nsec as u64)
}

/// Parse a line (like "VmRSS:  1234 kB") to extract the numeric value (1234).
pub fn parse_line(line: &str) -> Option<u64> {
    // We'll scan until we find a digit, then parse as an integer.
    // This is roughly the same idea as the original C code.
    let trimmed = line.trim();
    let digits_start = trimmed.find(|c: char| c.is_ascii_digit())?;
    // Slice off everything up to the first digit.
    let digits_part = &trimmed[digits_start..];
    // In C, the code ends by removing the trailing " kB", 
    // here we can simply parse up to whitespace or parse the full substring ignoring the "kB"
    digits_part
        .split_whitespace()
        .next()
        .and_then(|num_str| num_str.parse::<u64>().ok())
}

/// Return the `VmRSS` value (in kB) from `/proc/self/status`.
pub fn get_mem_value() -> Option<u64> {
    let file = File::open("/proc/self/status").ok()?;
    for line in BufReader::new(file).lines().flatten() {
        // The C code checks if line starts with "VmRSS:"
        if line.starts_with("VmRSS:") {
            return parse_line(&line);
        }
    }
    None
}


/// Analogous to the C macro:
/// ```c
/// BETTER_TEST_LOG(format, ...)
/// ```
/// In Rust, we use a `macro_rules!` macro to gather the format string and arguments.
#[macro_export]
macro_rules! better_test_log {
    ($fmt:literal $(, $args:expr)*) => {{
        let now_ms = crate::current_time_ns() / 1_000_000;
        eprintln!("LOG@{} {}:{} [<fn>] {}", 
            now_ms, 
            file!(), 
            line!(),
            format!($fmt, $($args),*)
        );
    }};
    ($fmt:expr, $($args:expr),*) => {{
        let now_ms = crate::current_time_ns() / 1_000_000;
        eprintln!("LOG@{} {}:{} [<fn>] {}", 
            now_ms, 
            file!(), 
            line!(),
            format!($fmt, $($args),*)
        );
    }};
}

pub fn version_string() -> String {
    let mut sys = sysinfo::System::new();
    sys.refresh_memory();
    sys.refresh_cpu_all();
    let sn = sysinfo::System::name().unwrap_or("".into()).replace("|","").replace("=","");
    let hn = sysinfo::System::host_name().unwrap_or("".into()).replace("|","").replace("=","");

    let lcores = sys.cpus().len();
    let pcores = num_cpus::get_physical();
    let total_mem = sys.total_memory();
    let used_mem = sys.used_memory();
    let total_swap = sys.total_swap();
    let used_swap = sys.used_swap();


    format!(" sys_name := {} | sys_hostname := {} | sys_lcores := {} | sys_pcores := {} | sys_total_mem_b := {} | sys_used_mem_b := {} | sys_total_swap_b := {} | sys_used_swap_b := {} ", sn, hn, lcores, pcores, total_mem, used_mem, total_swap, used_swap)
}

/// Rough equivalent of
/// ```c
/// #define REPORT_LINE(BENCH, IMPL, S, ...)
///     BETTER_TEST_LOG("\nREPORT_123" BENCH "|" IMPL "|" S "REPORT_123\n", ...)
/// ```
#[macro_export]
macro_rules! report_line {
    ($bench:expr, $impl:expr, $s:expr $(, $args:expr)*) => {{
        use common::version_string;
        $crate::better_test_log!(
            "\nREPORT_123{}|{}|{}| {}REPORT_123\n",
            $bench,
            $impl,
            version_string(),
            format!($s, $($args),*)
        );
    }};
    ($bench:expr, $impl:expr, $s:expr) => {{
        use common::version_string;
        $crate::better_test_log!(
            "\nREPORT_123{}|{}|{}| {}REPORT_123\n",
            $bench,
            $impl,
            version_string(),
            $s
        );
    }};
}


/// Equivalent to the RUN_TEST_FORKED(x) macro:
/// - Fork a child
/// - Child prints test name, runs function, exit(0) on success, or exit(errorCode) on fail
/// - Parent waits and prints appropriate success/fail message.
#[cfg(unix)]
pub fn run_test_forked<F: FnOnce() -> i32>(test_name: &str, test_func: F) {
    unsafe {
        let pid = libc::fork();
        if pid < 0 {
            eprintln!("FAILED: {}(fork)", test_name);
            return;
        }
        if pid == 0 {
            // Child
            eprintln!("\nRunning test: {}...", test_name);
            let result = test_func();
            if result == 0 {
                eprintln!("  SUCCESS");
                process::exit(0);
            } else {
                eprintln!(" FAIL: {} ({})", test_name, result);
                process::exit(result);
            }
        } else {
            // Parent
            let mut status: i32 = 0;
            libc::waitpid(pid, &mut status as *mut i32, 0);
            if status == 0 {
                better_test_log!("OK");
            } else {
                // In C code: BETTER_TEST_LOG("FAILED: %s (%d)\n", #x, returnStatus);
                better_test_log!("FAILED: {} ({})", test_name, status);
            }
        }
    }
}