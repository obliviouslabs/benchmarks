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


#define RUN_TEST_FORKED(x)                              \
do {                                                    \
  const long long timeout_ms = 3LL*60LL * 60LL * 1000LL;    \
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
    fprintf(stderr, "FAILED: %s(fork)\n", #x);          \
  } else {                                              \
    int returnStatus;                                   \
    int r = waitpid_timeout_linux(childPid, &returnStatus, timeout_ms);  \
    if (r == -1) {                                      \
      BETTER_TEST_LOG("FAILED: %s (wait error: %d)\n", #x, errno); \
    } else if (r == 1) {                                \
      BETTER_TEST_LOG("FAILED: %s (timeout after 1h)\n", #x); \
    } else  {                                           \
      if (returnStatus == 0) {                          \
        BETTER_TEST_LOG("OK\n");                        \
      } else {                                          \
        BETTER_TEST_LOG("FAILED: %s (%d)\n", #x, returnStatus);  \
      }                                                 \
    }                                                   \
  }                                                     \
} while(0)
