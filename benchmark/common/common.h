#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h> 
#include <time.h>
#include "string.h"
#include <unistd.h>
#include <sys/wait.h>


#include <algorithm>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <thread>
#include <sys/sysinfo.h>   // memory + swap
#include <sys/utsname.h>   // system / host name

// ----- helpers ---------------------------------------------------------------

// Count distinct (physical-id, core-id) pairs in /proc/cpuinfo.
// Falls back to logical-cores if parsing fails.
static std::uint32_t get_physical_cores()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo) return std::thread::hardware_concurrency();

    std::set<std::pair<int, int>> cores;
    std::string line;
    int physical_id = -1, core_id = -1;

    while (std::getline(cpuinfo, line))
    {
        if (line.rfind("physical id", 0) == 0)
            physical_id = std::stoi(line.substr(line.find(':') + 1));
        else if (line.rfind("core id", 0) == 0)
            core_id = std::stoi(line.substr(line.find(':') + 1));

        if (physical_id >= 0 && core_id >= 0)
        {
            cores.emplace(physical_id, core_id);
            physical_id = core_id = -1;
        }
    }
    return cores.empty() ? std::thread::hardware_concurrency()
                         : static_cast<std::uint32_t>(cores.size());
}

// Strip the characters ‘|’ and ‘=’ (mirrors the Rust .replace() chain).
static void sanitize(std::string& s)
{
    s.erase(std::remove_if(s.begin(), s.end(),
              [](char c) { return c == '|' || c == '='; }),
            s.end());
}

// ----- main API --------------------------------------------------------------

std::string version_string()
{
    // System + host names
    utsname uts{};
    uname(&uts);
    std::string sys_name  = uts.sysname;
    std::string host_name = uts.nodename;
    sanitize(sys_name);
    sanitize(host_name);

    // Memory + swap
    struct sysinfo info{};
    ::sysinfo(&info);
    const auto mem_unit   = static_cast<std::uint64_t>(info.mem_unit);
    std::uint64_t total_mem  = info.totalram  * mem_unit;
    std::uint64_t used_mem   = (info.totalram - info.freeram) * mem_unit;
    std::uint64_t total_swap = info.totalswap * mem_unit;
    std::uint64_t used_swap  = (info.totalswap - info.freeswap) * mem_unit;

    // CPU counts
    std::uint32_t lcores = std::thread::hardware_concurrency();
    std::uint32_t pcores = get_physical_cores();

    // Assemble exactly the same format as the Rust function
    std::ostringstream out;
    out << " sys_name := "        << sys_name
        << " | sys_hostname := "  << host_name
        << " | sys_lcores := "    << lcores
        << " | sys_pcores := "    << pcores
        << " | sys_total_mem_b := "  << total_mem
        << " | sys_used_mem_b := "   << used_mem
        << " | sys_total_swap_b := " << total_swap
        << " | sys_used_swap_b := "  << used_swap
        << " ";

    return out.str();
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
  BETTER_TEST_LOG("\nREPORT_123" BENCH "|" IMPL "|%s| " S "REPORT_123\n", version_string().c_str(), ##__VA_ARGS__); \
} while(0)




#define RUN_TEST_FORKED(x)                              \
do {                                                    \
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
    waitpid(childPid, &returnStatus, 0);                \
    if (returnStatus == 0) {                            \
      /* Child process terminated without error */      \
      BETTER_TEST_LOG("OK\n");                                 \
    } else {                                            \
      /* Child process terminated with an error*/       \
      BETTER_TEST_LOG("FAILED: %s (%d)\n", #x, returnStatus);  \
    }                                                   \
  }                                                     \
} while(0)
