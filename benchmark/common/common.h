#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/file.h>
#include <stdarg.h>
#include <ctype.h>

// ----- helpers ---------------------------------------------------------------

// Count distinct (physical-id, core-id) pairs in /proc/cpuinfo.
// Falls back to logical-cores if parsing fails.
static uint32_t get_physical_cores(void)
{
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp)
        return (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);

    struct { int phys, core; } seen[1024];
    size_t n_seen = 0;
    char  *line   = NULL;
    size_t cap    = 0;
    int phys_id   = -1, core_id = -1;

    while (getline(&line, &cap, fp) != -1) {
        if (strncmp(line, "physical id", 11) == 0)
            phys_id = (int)strtol(strchr(line, ':') + 1, NULL, 10);
        else if (strncmp(line, "core id", 7) == 0)
            core_id = (int)strtol(strchr(line, ':') + 1, NULL, 10);

        if (phys_id >= 0 && core_id >= 0) {
            size_t i;
            for (i = 0; i < n_seen; ++i)
                if (seen[i].phys == phys_id && seen[i].core == core_id)
                    break;
            if (i == n_seen && n_seen < sizeof seen / sizeof *seen) {
                seen[n_seen].phys = phys_id;
                seen[n_seen].core = core_id;
                ++n_seen;
            }
            phys_id = core_id = -1;
        }
    }
    free(line);
    fclose(fp);
    return n_seen ? (uint32_t)n_seen
                  : (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
}

// Strip the characters ‘|’ and ‘=’ (mirrors the Rust .replace() chain).
static void sanitize(char *s)
{
    char *dst = s, *src = s;
    while (*src) {
        if (*src != '|' && *src != '=')
            *dst++ = *src;
        ++src;
    }
    *dst = '\0';
}

const char *version_string(void)
{
    static char buf[512];

    /* ---------- system & host names ---------- */
    struct utsname uts;
    uname(&uts);
    sanitize(uts.sysname);
    sanitize(uts.nodename);

    /* ---------- memory & swap --------------- */
    struct sysinfo info;
    sysinfo(&info);
    uint64_t unit       = info.mem_unit;
    uint64_t total_mem  = info.totalram            * unit;
    uint64_t used_mem   = (info.totalram - info.freeram)   * unit;
    uint64_t total_swap = info.totalswap           * unit;
    uint64_t used_swap  = (info.totalswap - info.freeswap) * unit;

    /* ---------- CPU counts ------------------- */
    uint32_t lcores = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
    uint32_t pcores = get_physical_cores();

    /* ---------- assemble string -------------- */
    snprintf(buf, sizeof buf,
             " sys_name := %s"
             " | sys_hostname := %s"
             " | sys_lcores := %" PRIu32
             " | sys_pcores := %" PRIu32
             " | sys_total_mem_b := %" PRIu64
             " | sys_used_mem_b := %" PRIu64
             " | sys_total_swap_b := %" PRIu64
             " | sys_used_swap_b := %" PRIu64
             " ",
             uts.sysname,
             uts.nodename,
             lcores,
             pcores,
             total_mem,
             used_mem,
             total_swap,
             used_swap);

    return buf;
}

static inline uint64_t current_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}



static uint64_t parseLine(char* line){
  // This assumes that a digit will be found and the line ends in " Kb".
  uint64_t i = strlen(line);
  const char* p = line;
  while (*p <'0' || *p > '9') p++;
  line[i-3] = '\0';
  i = strtoull(p, NULL, 10);
  return i;
}

static uint64_t getMemValue(){ //Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  uint64_t result = -1;
  char line[128];

  while (fgets(line, 128, file) != NULL){
      if (strncmp(line, "VmRSS:", 6) == 0){
          result = parseLine(line);
          break;
      }
  }
  fclose(file);
  return result;
}

#define BETTER_TEST_LOG(format, ...) do { \
  char _buf[1024] = {0}; \
  snprintf(_buf, sizeof(_buf), "LOG@%ld %s:%03d [%s] " format "\n", current_time_ns()/1000000, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
  fprintf(stderr, "%s", _buf); \
} while (0)


#define REPORT_LINE(BENCH, IMPL, S, ...) do { \
  BETTER_TEST_LOG("\nREPORT_123" BENCH "|" IMPL "|%s| " S "REPORT_123\n", version_string(), ##__VA_ARGS__); \
} while(0)

static int pidfd_open_linux(pid_t pid, unsigned int flags) {
  return (int)syscall(SYS_pidfd_open, pid, flags);
}

static long long benchmark_test_timeout_ms(void)
{
  const char *raw = getenv("BENCHMARK_TEST_TIMEOUT_MS");
  const long long default_timeout_ms = 2LL * 60LL * 60LL * 1000LL;

  if (raw == NULL || raw[0] == '\0')
  {
    return default_timeout_ms;
  }

  errno = 0;
  char *end = NULL;
  unsigned long long parsed = strtoull(raw, &end, 10);
  if (end == raw || *end != '\0' || errno != 0)
  {
    return default_timeout_ms;
  }

  return (long long)parsed;
}

// returns: 0 = exited, 1 = timeout (child killed), -1 = error
static int waitpid_timeout_linux(pid_t pid, int *status, int timeout_ms) {
  int pfd = pidfd_open_linux(pid, 0);
  if (pfd < 0) return -1;

  struct pollfd fds = { .fd = pfd, .events = POLLIN };
  int pr = poll(&fds, 1, timeout_ms);

  if (pr == 0) {
    // Timeout
    kill(pid, SIGTERM);
    usleep(200 * 1000);
    kill(pid, SIGKILL);

    (void)waitpid(pid, status, 0);
    close(pfd);
    return 1;
  }

  if (pr < 0) {
    // Wait error
    close(pfd);
    return -1;
  }

  // child has exited (or is waitable);
  if (waitpid(pid, status, 0) < 0) {
    close(pfd);
    return -1;
  }

  close(pfd);
  return 0;
}


/* One append-only JSONL channel; no test registry or runner-specific parsing. */
static int benchmark_planning(void) {
  const char *mode = getenv("BENCHMARK_MODE");
  if (!mode || !*mode || !strcmp(mode, "run")) return 0;
  if (!strcmp(mode, "plan")) return 1;
  fprintf(stderr, "Invalid BENCHMARK_MODE: %s\n", mode);
  exit(2);
}

static void benchmark_json(FILE *out, const char *s) {
  fputc('"', out);
  for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; ++p) {
    if (*p == '"' || *p == '\\') fprintf(out, "\\%c", *p);
    else if (*p < 32) fprintf(out, "\\u%04x", *p);
    else fputc(*p, out);
  }
  fputc('"', out);
}

static FILE *benchmark_log_open(void) {
  const char *path = getenv("BENCHMARK_LOG_FILE");
  if (!path || !*path) return stdout;
  FILE *out = fopen(path, "a");
  if (!out || flock(fileno(out), LOCK_EX)) {
    perror("BENCHMARK_LOG_FILE");
    exit(2);
  }
  return out;
}

static void benchmark_log_close(FILE *out) {
  int failed = ferror(out) || fflush(out);
  if (out != stdout && fclose(out)) failed = 1;
  if (failed) { perror("BENCHMARK_LOG_FILE"); exit(2); }
}

/* Selector fields are readable text, escaped only to keep tabs/control bytes on one line. */
static int benchmark_selector_hex(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int benchmark_selector_unescape(char *field, size_t *length) {
  char *out = field;
  for (const char *p = field; *p;) {
    unsigned char c = (unsigned char)*p++;
    if (c == '\\') {
      c = (unsigned char)*p++;
      if (c == 'n') c = '\n';
      else if (c == 'r') c = '\r';
      else if (c == 't') c = '\t';
      else if (c == 'x') {
        if (!p[0] || !p[1]) return 0;
        int hi = benchmark_selector_hex((unsigned char)p[0]);
        int lo = benchmark_selector_hex((unsigned char)p[1]);
        if (hi < 0 || hi > 7 || lo < 0) return 0;
        c = (unsigned char)(hi * 16 + lo);
        p += 2;
      } else if (c != '\\') return 0;
    } else if (c < 32 || c == 127) return 0;
    *out++ = (char)c;
  }
  *length = (size_t)(out - field);
  *out = '\0';
  return 1;
}

static const char *benchmark_selection(const char *wanted[4]) {
  const char *path = getenv("BENCHMARK_SELECTOR");
  if (!path || !*path) return "start";
  FILE *in = fopen(path, "r");
  if (!in) { perror("BENCHMARK_SELECTOR"); exit(2); }
  const char *result = "skipped_missing";
  char *text = NULL;
  size_t capacity = 0, number = 0;
  ssize_t length;
  int found = 0;
  while ((length = getline(&text, &capacity, in)) >= 0) {
    ++number;
    int valid = (size_t)length == strlen(text);
    if (length && text[length - 1] == '\n') text[--length] = '\0';
    if (length && text[length - 1] == '\r') text[--length] = '\0';
    char *p = text;
    while (isspace((unsigned char)*p)) ++p;
    if (!*p && valid) continue;
    char *columns[6] = {p};
    size_t lengths[4];
    int count = 1;
    for (char *q = p; *q && count < 6; ++q) {
      if (*q == '\t') { *q = '\0'; columns[count++] = q + 1; }
    }
    int commented = *p == '#';
    while (*p == '#') {
      ++p;
      while (isspace((unsigned char)*p)) ++p;
    }
    char *action = p;
    while (*p && !isspace((unsigned char)*p)) ++p;
    char *end = p;
    while (isspace((unsigned char)*p)) ++p;
    if (*p || (end == action && !commented)) valid = 0;
    *end = '\0';
    if (count < 5) valid = 0;
    if (count == 6) {
      p = columns[5];
      while (isspace((unsigned char)*p)) ++p;
      if (*p != '#') valid = 0;
    }
    if (valid) {
      for (int i = 0; i < 4; ++i)
        if (!benchmark_selector_unescape(columns[i + 1], &lengths[i])) valid = 0;
    }
    if (!valid) {
      if (commented) continue;
      fprintf(stderr, "Invalid selector entry in %s:%zu; expected tab-separated action, project, variant, name, params\n", path, number);
      exit(2);
    }
    int matches = 1;
    for (int i = 0; i < 4; ++i) {
      const char *value = wanted[i] ? wanted[i] : "";
      if (lengths[i] != strlen(value) || memcmp(columns[i + 1], value, lengths[i])) matches = 0;
    }
    if (matches) {
      if (found++) { fprintf(stderr, "Duplicate selector case in %s:%zu\n", path, number); exit(2); }
      result = (!commented && (!strcmp(action, "run") || !strcmp(action, "yes"))) ? "start" : "skipped";
    }
  }
  if (ferror(in)) { perror("BENCHMARK_SELECTOR"); exit(2); }
  free(text);
  fclose(in);
  return result;
}

static uint64_t benchmark_start(const char *name, const char *file, int line,
                                const char *format, ...) {
  static uint64_t sequence = 0;
  uint64_t id = ++sequence;
  int plan = benchmark_planning();
  char params[1024];
  va_list args;
  va_start(args, format);
  int length = vsnprintf(params, sizeof(params), format, args);
  va_end(args);
  if (length < 0 || (size_t)length >= sizeof(params)) {
    fprintf(stderr, "Benchmark parameters exceed log buffer\n");
    exit(2);
  }
  const char *fields[] = {getenv("BENCHMARK_PROJECT"), getenv("BENCHMARK_VARIANT"), name, params};
  const char *event = plan ? "plan" : benchmark_selection(fields);
  int run = !strcmp(event, "start");
  int missing = !strcmp(event, "skipped_missing");
  FILE *out = benchmark_log_open();
  fprintf(out, "{\"event\":\"%s\",\"id\":\"%ld:%" PRIu64 "\",\"name\":",
          event, (long)getpid(), id);
  benchmark_json(out, name);
  fputs(",\"params\":", out); benchmark_json(out, params);
  fputs(",\"file\":", out); benchmark_json(out, file);
  fputs(",\"project\":", out); benchmark_json(out, getenv("BENCHMARK_PROJECT"));
  fputs(",\"variant\":", out); benchmark_json(out, getenv("BENCHMARK_VARIANT"));
  fputs(",\"run\":", out); benchmark_json(out, getenv("BENCHMARK_RUN"));
  fprintf(out, ",\"line\":%d", line);
  if (missing) fputs(",\"warning\":\"not present in selector; skipped\"", out);
  if (run) fprintf(out, ",\"time_ns\":%" PRIu64, current_time_ns());
  fputs("}\n", out);
  benchmark_log_close(out);  /* Flush before fork, including on redirected stdout. */
  if (missing) fprintf(stderr, "Warning: %s %s %s [%s] not present in selector; skipped\n",
                       fields[0] ? fields[0] : "", fields[1] ? fields[1] : "", name, params);
  return run ? id : 0;
}

static void benchmark_end(uint64_t id, int returncode, const char *error) {
  uint64_t now = current_time_ns();
  FILE *out = benchmark_log_open();
  fprintf(out, "{\"event\":\"end\",\"id\":\"%ld:%" PRIu64
          "\",\"time_ns\":%" PRIu64 ",\"returncode\":%d,\"error\":",
          (long)getpid(), id, now, returncode);
  benchmark_json(out, error);
  fputs("}\n", out);
  benchmark_log_close(out);
}

/* Optional printf-style parameters are evaluated before the test is started. */
#define RUN_TEST_FORKED(x, ...)                         \
do {                                                    \
  uint64_t _case_id = benchmark_start(#x, __FILE__, __LINE__, "" __VA_ARGS__); \
  if (!_case_id) break;                                  \
  const uint64_t _started_ns = current_time_ns();         \
  const long long timeout_ms = benchmark_test_timeout_ms(); \
  pid_t childPid = fork();                              \
  if (childPid == 0) {                                  \
    /* Child process */                                 \
    fprintf(stderr, "\nRunning test: %s...\n", #x);     \
    int _result = x;                                    \
    if (_result == 0) {                                 \
      fprintf(stderr, "  SUCCESS\n");                   \
      exit(0);                                          \
    } else {                                            \
      fprintf(stderr, " FAIL: %s (%d)\n", #x, _result); \
      exit(_result);                                    \
    }                                                   \
  } else if (childPid < 0) {                            \
    /* Fork failed */                                   \
    fprintf(stderr, "FAILED: %s (fork failed, elapsed=%.3fs)\n", #x, \
            (current_time_ns() - _started_ns) / 1e9);     \
    benchmark_end(_case_id, -1, "fork failed");           \
  } else {                                              \
    int returnStatus = 0;                               \
    int r = waitpid_timeout_linux(childPid, &returnStatus, timeout_ms);  \
    const double _elapsed_s = (current_time_ns() - _started_ns) / 1e9; \
    if (r == -1) {                                      \
      BETTER_TEST_LOG("FAILED: %s (wait error: %d, elapsed=%.3fs)", #x, errno, _elapsed_s); \
    } else if (r == 1) {                                \
      BETTER_TEST_LOG("FAILED: %s (timeout after %.3fs, elapsed=%.3fs)", #x, (double)timeout_ms / 1000.0, _elapsed_s); \
    } else  {                                           \
      if (returnStatus == 0) {                          \
        BETTER_TEST_LOG("OK: %s (elapsed=%.3fs)", #x, _elapsed_s); \
      } else {                                          \
        BETTER_TEST_LOG("FAILED: %s (%d, elapsed=%.3fs)", #x, returnStatus, _elapsed_s);  \
      }                                                 \
    }                                                   \
    int _code = WIFEXITED(returnStatus) ? WEXITSTATUS(returnStatus) : -WTERMSIG(returnStatus); \
    benchmark_end(_case_id, r ? -1 : _code, r == 1 ? "timeout" : r == -1 ? "wait failed" : ""); \
  }                                                     \
} while(0)
