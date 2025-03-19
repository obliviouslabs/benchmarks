#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h> 
#include <time.h>
#include "string.h"
#include <unistd.h>
#include <sys/wait.h>

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
  BETTER_TEST_LOG("\nREPORT_123" BENCH "|" IMPL "|" S "REPORT_123\n", ##__VA_ARGS__); \
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
