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


pub fn benchmark_planning() -> bool {
    match std::env::var("BENCHMARK_MODE").as_deref() {
        Err(_) | Ok("") | Ok("run") => false,
        Ok("plan") => true,
        Ok(mode) => panic!("Invalid BENCHMARK_MODE: {}", mode),
    }
}

fn benchmark_json(value: &str) -> String {
    let mut out = String::from("\"");
    for c in value.chars() {
        match c {
            '"' | '\\' => { out.push('\\'); out.push(c); }
            '\0'..='\u{1f}' => out.push_str(&format!("\\u{:04x}", c as u32)),
            _ => out.push(c),
        }
    }
    out.push('"');
    out
}

fn benchmark_emit(record: String) {
    use std::io::Write;
    use std::os::unix::io::AsRawFd;
    if let Some(path) = std::env::var_os("BENCHMARK_LOG_FILE").filter(|p| !p.is_empty()) {
        let mut out = std::fs::OpenOptions::new().append(true).create(true)
            .open(path).expect("BENCHMARK_LOG_FILE");
        assert_eq!(unsafe { libc::flock(out.as_raw_fd(), libc::LOCK_EX) }, 0);
        out.write_all(record.as_bytes()).expect("BENCHMARK_LOG_FILE");
    } else {
        std::io::stdout().write_all(record.as_bytes()).expect("benchmark log");
    }
}

fn benchmark_selector_unescape(value: &str) -> Option<String> {
    let mut out = String::new();
    let mut chars = value.chars();
    while let Some(c) = chars.next() {
        let decoded = if c == '\\' {
            match chars.next()? {
                '\\' => '\\', 'n' => '\n', 'r' => '\r', 't' => '\t',
                'x' => {
                    let code = chars.next()?.to_digit(16)? * 16 + chars.next()?.to_digit(16)?;
                    if code > 127 { return None; }
                    char::from_u32(code)?
                },
                _ => return None,
            }
        } else {
            if (c as u32) < 32 || c == '\u{7f}' { return None; }
            c
        };
        out.push(decoded);
    }
    Some(out)
}

fn benchmark_selection(wanted: &[&str; 4]) -> &'static str {
    let Some(path) = std::env::var_os("BENCHMARK_SELECTOR").filter(|p| !p.is_empty()) else {
        return "start";
    };
    let input = File::open(path).expect("BENCHMARK_SELECTOR");
    let mut choice = None;
    let whitespace = |c| matches!(c, ' ' | '\t' | '\r' | '\n' | '\u{b}' | '\u{c}');
    for (number, line) in BufReader::new(input).lines().enumerate() {
        let line = line.expect("BENCHMARK_SELECTOR");
        let text = line.strip_suffix('\r').unwrap_or(&line).trim_start_matches(whitespace);
        if text.is_empty() { continue; }
        let columns: Vec<_> = text.splitn(6, '\t').collect();
        let mut action = columns[0].trim_matches(whitespace);
        let commented = action.starts_with('#');
        while let Some(rest) = action.strip_prefix('#') { action = rest.trim_matches(whitespace); }
        let values = if !text.contains('\0') && columns.len() >= 5
            && (columns.len() == 5 || columns[5].trim_start_matches(whitespace).starts_with('#'))
            && !action.chars().any(whitespace) && (!action.is_empty() || commented) {
            columns[1..5].iter().map(|field| benchmark_selector_unescape(field)).collect::<Option<Vec<_>>>()
        } else { None };
        let Some(values) = values else {
            if commented { continue; }
            panic!("invalid selector line {}; expected tab-separated action, project, variant, name, params", number + 1);
        };
        if values.iter().map(String::as_str).eq(wanted.iter().copied()) {
            assert!(choice.is_none(), "duplicate selector case on line {}: {:?}", number + 1, wanted);
            choice = Some(if !commented && matches!(action, "run" | "yes") { "start" } else { "skipped" });
        }
    }
    choice.unwrap_or("skipped_missing")
}

#[track_caller]
pub fn benchmark_start(name: &str, params: &str) -> Option<String> {
    use std::sync::atomic::{AtomicU64, Ordering};
    static SEQUENCE: AtomicU64 = AtomicU64::new(1);
    let id = format!("{}:{}", process::id(), SEQUENCE.fetch_add(1, Ordering::Relaxed));
    let caller = std::panic::Location::caller();
    let project = std::env::var("BENCHMARK_PROJECT").unwrap_or_default();
    let variant = std::env::var("BENCHMARK_VARIANT").unwrap_or_default();
    let event = if benchmark_planning() { "plan" }
                else { benchmark_selection(&[&project, &variant, name, params]) };
    let warning = if event == "skipped_missing" {
        ",\"warning\":\"not present in selector; skipped\""
    } else { "" };
    let time = if event == "start" { format!(",\"time_ns\":{}", current_time_ns()) } else { String::new() };
    benchmark_emit(format!(
        "{{\"event\":\"{}\",\"id\":\"{}\",\"name\":{},\"params\":{},\"file\":{},\"line\":{},\"project\":{},\"variant\":{},\"run\":{}{}{}}}\n",
        event, id, benchmark_json(name), benchmark_json(params),
        benchmark_json(caller.file()), caller.line(), benchmark_json(&project), benchmark_json(&variant),
        benchmark_json(&std::env::var("BENCHMARK_RUN").unwrap_or_default()), time, warning));
    if event == "skipped_missing" {
        eprintln!("Warning: {project} {variant} {name} [{params}] not present in selector; skipped");
    }
    if event == "start" { Some(id) } else { None }
}

pub fn benchmark_end(id: &str, returncode: i32, error: &str) {
    benchmark_emit(format!("{{\"event\":\"end\",\"id\":{},\"time_ns\":{},\"returncode\":{},\"error\":{}}}\n",
                           benchmark_json(id), current_time_ns(), returncode, benchmark_json(error)));
}

/// Equivalent to the RUN_TEST_FORKED(x) macro:
/// - Fork a child
/// - Child prints test name, runs function, exit(0) on success, or exit(errorCode) on fail
/// - Parent waits and prints appropriate success/fail message.
#[cfg(unix)]
#[track_caller]
pub fn run_test_forked<F: FnOnce() -> i32>(test_name: &str, params: &str, test_func: F) {
    let Some(id) = benchmark_start(test_name, params) else { return; };
    unsafe {
        let pid = libc::fork();
        if pid < 0 {
            eprintln!("FAILED: {}(fork)", test_name);
            benchmark_end(&id, -1, "fork failed");
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
            loop {
                if libc::waitpid(pid, &mut status as *mut i32, 0) >= 0 { break; }
                if std::io::Error::last_os_error().raw_os_error() != Some(libc::EINTR) {
                    benchmark_end(&id, -1, "wait failed");
                    return;
                }
            }
            let code = if libc::WIFEXITED(status) { libc::WEXITSTATUS(status) }
                       else { -libc::WTERMSIG(status) };
            benchmark_end(&id, code, "");
            if status == 0 {
                better_test_log!("OK");
            } else {
                // In C code: BETTER_TEST_LOG("FAILED: %s (%d)\n", #x, returnStatus);
                better_test_log!("FAILED: {} ({})", test_name, status);
            }
        }
    }
}