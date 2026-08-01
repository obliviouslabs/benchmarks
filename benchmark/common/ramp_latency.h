#ifndef OBLIVIOUS_BENCHMARK_RAMP_LATENCY_H
#define OBLIVIOUS_BENCHMARK_RAMP_LATENCY_H

/*
 * Open-loop latency benchmark for a synchronous map implementation.
 *
 * A producer publishes requests according to an absolute monotonic-clock
 * schedule. The calling thread consumes them one at a time. Consequently, a
 * slow lookup leaves later requests in the queue and its reported
 * incoming-to-responded latency includes that queueing delay.
 *
 * The hot path only reads the monotonic clock and writes pre-faulted memory.
 * CSV and metadata files are opened after the producer has stopped and the
 * queue has drained.
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ramp_latency_query_fn)(void *context, uint64_t query_id);

typedef struct ramp_latency_record {
    uint64_t incoming_ns;
    uint64_t responded_ns;
} ramp_latency_record;

typedef struct ramp_latency_config {
    uint64_t map_size;
    uint64_t min_queries;
    uint64_t max_queries;
    uint64_t phase_queries;
    uint64_t calibration_queries;
    uint32_t overload_phases;
    double start_fraction;
    double step_fraction;
    const char *implementation;
    const char *csv_path;
} ramp_latency_config;

typedef struct ramp_latency_summary {
    uint64_t queries;
    uint64_t elapsed_ns;
    double calibration_qps;
    double final_target_qps;
    double average_latency_ns;
    uint64_t p50_ns;
    uint64_t p95_ns;
    uint64_t p99_ns;
    uint64_t p999_ns;
    uint64_t max_ns;
    const char *stop_reason;
} ramp_latency_summary;

static uint64_t ramp_latency_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static uint64_t ramp_latency_env_u64(const char *name, uint64_t fallback) {
    const char *raw = getenv(name);
    char *end = NULL;
    unsigned long long parsed;
    if (raw == NULL || raw[0] == '\0')
        return fallback;
    errno = 0;
    parsed = strtoull(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0')
        return fallback;
    return (uint64_t)parsed;
}

static double ramp_latency_env_double(const char *name, double fallback) {
    const char *raw = getenv(name);
    char *end = NULL;
    double parsed;
    if (raw == NULL || raw[0] == '\0')
        return fallback;
    errno = 0;
    parsed = strtod(raw, &end);
    if (errno != 0 || end == raw || *end != '\0' || !isfinite(parsed))
        return fallback;
    return parsed;
}

static uint64_t ramp_latency_saturating_mul(uint64_t a, uint64_t b) {
    if (a != 0 && b > UINT64_MAX / a)
        return UINT64_MAX;
    return a * b;
}

static void ramp_latency_default_config(ramp_latency_config *config, uint64_t map_size,
                                        const char *implementation, const char *csv_path) {
    uint64_t phase_default = map_size / 8;
    uint64_t calibration_default = map_size;
    uint64_t min_factor = ramp_latency_env_u64("RAMP_MIN_QUERIES_FACTOR", 3);
    uint64_t max_factor = ramp_latency_env_u64("RAMP_MAX_QUERIES_FACTOR", 8);

    if (phase_default < 64)
        phase_default = 64;
    if (calibration_default < 4096)
        calibration_default = 4096;
    if (calibration_default > 16384)
        calibration_default = 16384;

    config->map_size = map_size;
    config->min_queries = ramp_latency_saturating_mul(map_size, min_factor);
    config->max_queries = ramp_latency_saturating_mul(map_size, max_factor);
    config->min_queries = ramp_latency_env_u64("RAMP_MIN_QUERIES", config->min_queries);
    config->max_queries = ramp_latency_env_u64("RAMP_MAX_QUERIES", config->max_queries);
    if (config->max_queries < config->min_queries)
        config->max_queries = config->min_queries;
    config->phase_queries = ramp_latency_env_u64("RAMP_PHASE_QUERIES", phase_default);
    if (config->phase_queries == 0)
        config->phase_queries = 1;
    config->calibration_queries =
        ramp_latency_env_u64("RAMP_CALIBRATION_QUERIES", calibration_default);
    if (config->calibration_queries == 0)
        config->calibration_queries = 1;
    config->overload_phases = (uint32_t)ramp_latency_env_u64("RAMP_OVERLOAD_PHASES", 2);
    if (config->overload_phases == 0)
        config->overload_phases = 1;
    config->start_fraction = ramp_latency_env_double("RAMP_START_FRACTION", 0.25);
    config->step_fraction = ramp_latency_env_double("RAMP_STEP_FRACTION", 0.025);
    if (config->start_fraction <= 0.0)
        config->start_fraction = 0.25;
    if (config->step_fraction <= 0.0)
        config->step_fraction = 0.025;
    config->implementation = implementation;
    config->csv_path = csv_path;
}

static int ramp_latency_calibrate(void *context, ramp_latency_query_fn query, uint64_t query_count,
                                  double *qps_out) {
    uint64_t i;
    /* Warm the map before using the same-sized second pass for calibration. */
    for (i = 0; i < query_count; ++i) {
        int status = query(context, i);
        if (status != 0)
            return status;
    }

    uint64_t start_ns = ramp_latency_now_ns();
    for (i = 0; i < query_count; ++i) {
        int status = query(context, i);
        if (status != 0)
            return status;
    }
    uint64_t elapsed_ns = ramp_latency_now_ns() - start_ns;
    if (elapsed_ns == 0)
        elapsed_ns = 1;
    *qps_out = (double)query_count * 1e9 / (double)elapsed_ns;
    return 0;
}

typedef struct ramp_latency_shared {
    ramp_latency_record *records;
    const ramp_latency_config *config;
    double calibration_qps;
    uint64_t start_ns;
    uint64_t produced;
    uint64_t responded;
    uint64_t service_ns;
    int producer_done;
    int query_error;
    uint64_t final_queries;
    double final_target_qps;
    const char *stop_reason;
} ramp_latency_shared;

static void ramp_latency_cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#else
    sched_yield();
#endif
}

static void ramp_latency_wait_until(uint64_t target_ns) {
    const uint64_t sleep_margin_ns = UINT64_C(25000);
    for (;;) {
        uint64_t now_ns = ramp_latency_now_ns();
        if (now_ns >= target_ns)
            return;
        if (target_ns - now_ns > UINT64_C(50000)) {
            uint64_t wake_ns = target_ns - sleep_margin_ns;
            struct timespec wake;
            wake.tv_sec = (time_t)(wake_ns / UINT64_C(1000000000));
            wake.tv_nsec = (long)(wake_ns % UINT64_C(1000000000));
            while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wake, NULL) == EINTR)
                ;
        } else {
            ramp_latency_cpu_relax();
        }
    }
}

static void *ramp_latency_producer(void *raw_shared) {
    ramp_latency_shared *shared = (ramp_latency_shared *)raw_shared;
    const ramp_latency_config *config = shared->config;
    long double scheduled_offset_ns = 0.0L;
    uint64_t query_id = 0;
    uint64_t current_phase = UINT64_MAX;
    uint32_t overloaded_checks = 0;
    double target_qps = shared->calibration_qps * config->start_fraction;

    while (query_id < config->max_queries) {
        uint64_t phase = query_id / config->phase_queries;
        if (phase != current_phase) {
            uint64_t responded = __atomic_load_n(&shared->responded, __ATOMIC_ACQUIRE);
            uint64_t service_ns = __atomic_load_n(&shared->service_ns, __ATOMIC_ACQUIRE);
            uint64_t backlog = query_id > responded ? query_id - responded : 0;
            double service_qps =
                service_ns == 0 ? INFINITY : (double)responded * 1e9 / (double)service_ns;

            target_qps = shared->calibration_qps *
                         (config->start_fraction + config->step_fraction * (double)phase);
            current_phase = phase;

            if (query_id >= config->min_queries && backlog >= config->phase_queries &&
                target_qps > service_qps) {
                ++overloaded_checks;
            } else {
                overloaded_checks = 0;
            }

            if (overloaded_checks >= config->overload_phases) {
                shared->stop_reason = "sustained_overload";
                break;
            }
        }

        scheduled_offset_ns += 1e9L / (long double)target_qps;
        uint64_t scheduled_ns = shared->start_ns + (uint64_t)scheduled_offset_ns;
        ramp_latency_wait_until(scheduled_ns);

        if (__atomic_load_n(&shared->query_error, __ATOMIC_ACQUIRE) != 0) {
            shared->stop_reason = "query_error";
            break;
        }

        shared->records[query_id].incoming_ns = ramp_latency_now_ns() - shared->start_ns;
        __atomic_store_n(&shared->produced, query_id + 1, __ATOMIC_RELEASE);
        ++query_id;
    }

    shared->final_queries = query_id;
    shared->final_target_qps = target_qps;
    if (query_id == config->max_queries && shared->stop_reason == NULL)
        shared->stop_reason = "max_queries";
    __atomic_store_n(&shared->producer_done, 1, __ATOMIC_RELEASE);
    return NULL;
}

static int ramp_latency_compare_record(const void *left, const void *right) {
    uint64_t a = ((const ramp_latency_record *)left)->responded_ns;
    uint64_t b = ((const ramp_latency_record *)right)->responded_ns;
    return (a > b) - (a < b);
}

static uint64_t ramp_latency_percentile(const ramp_latency_record *sorted, uint64_t count,
                                        double percentile) {
    uint64_t rank;
    double exact_rank;
    if (count == 0)
        return 0;
    exact_rank = percentile * (double)count;
    rank = (uint64_t)exact_rank;
    if ((double)rank < exact_rank)
        ++rank;
    if (rank == 0)
        rank = 1;
    if (rank > count)
        rank = count;
    return sorted[rank - 1].responded_ns;
}

static int ramp_latency_write_results(ramp_latency_shared *shared, ramp_latency_summary *summary) {
    const ramp_latency_config *config = shared->config;
    uint64_t count = shared->final_queries;
    long double latency_sum = 0.0L;
    uint64_t i;
    FILE *csv = NULL;
    FILE *meta = NULL;
    char *meta_path = NULL;
    size_t meta_path_len;

    memset(summary, 0, sizeof(*summary));
    summary->queries = count;
    summary->calibration_qps = shared->calibration_qps;
    summary->final_target_qps = shared->final_target_qps;
    summary->stop_reason = shared->stop_reason == NULL ? "unknown" : shared->stop_reason;
    if (count != 0)
        summary->elapsed_ns = shared->records[count - 1].responded_ns;

    /* Preserve the raw records on disk before reusing their memory for sorting. */
    csv = fopen(config->csv_path, "w");
    if (csv == NULL)
        return errno == 0 ? EIO : errno;
    fprintf(csv, "query_id,incoming_ns,responded_ns\n");
    for (i = 0; i < count; ++i) {
        fprintf(csv, "%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", i, shared->records[i].incoming_ns,
                shared->records[i].responded_ns);
    }
    if (fclose(csv) != 0)
        return errno == 0 ? EIO : errno;

    for (i = 0; i < count; ++i) {
        uint64_t latency = shared->records[i].responded_ns - shared->records[i].incoming_ns;
        shared->records[i].responded_ns = latency;
        latency_sum += (long double)latency;
    }
    qsort(shared->records, (size_t)count, sizeof(*shared->records), ramp_latency_compare_record);
    if (count != 0) {
        summary->average_latency_ns = (double)(latency_sum / (long double)count);
        summary->p50_ns = ramp_latency_percentile(shared->records, count, 0.50);
        summary->p95_ns = ramp_latency_percentile(shared->records, count, 0.95);
        summary->p99_ns = ramp_latency_percentile(shared->records, count, 0.99);
        summary->p999_ns = ramp_latency_percentile(shared->records, count, 0.999);
        summary->max_ns = shared->records[count - 1].responded_ns;
    }

    meta_path_len = strlen(config->csv_path) + sizeof(".meta.json");
    meta_path = (char *)malloc(meta_path_len);
    if (meta_path == NULL)
        return ENOMEM;
    snprintf(meta_path, meta_path_len, "%s.meta.json", config->csv_path);
    meta = fopen(meta_path, "w");
    if (meta == NULL) {
        int saved_errno = errno == 0 ? EIO : errno;
        free(meta_path);
        return saved_errno;
    }
    fprintf(meta,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"implementation\": \"%s\",\n"
            "  \"map_size\": %" PRIu64 ",\n"
            "  \"queries\": %" PRIu64 ",\n"
            "  \"minimum_queries\": %" PRIu64 ",\n"
            "  \"maximum_queries\": %" PRIu64 ",\n"
            "  \"phase_queries\": %" PRIu64 ",\n"
            "  \"warmup_queries\": %" PRIu64 ",\n"
            "  \"calibration_queries\": %" PRIu64 ",\n"
            "  \"calibration_qps\": %.9f,\n"
            "  \"start_fraction\": %.9f,\n"
            "  \"step_fraction\": %.9f,\n"
            "  \"final_target_qps\": %.9f,\n"
            "  \"stop_reason\": \"%s\",\n"
            "  \"average_latency_ns\": %.3f,\n"
            "  \"p50_latency_ns\": %" PRIu64 ",\n"
            "  \"p95_latency_ns\": %" PRIu64 ",\n"
            "  \"p99_latency_ns\": %" PRIu64 ",\n"
            "  \"p999_latency_ns\": %" PRIu64 ",\n"
            "  \"max_latency_ns\": %" PRIu64 "\n"
            "}\n",
            config->implementation, config->map_size, summary->queries, config->min_queries,
            config->max_queries, config->phase_queries, config->calibration_queries,
            config->calibration_queries, summary->calibration_qps, config->start_fraction,
            config->step_fraction, summary->final_target_qps, summary->stop_reason,
            summary->average_latency_ns, summary->p50_ns, summary->p95_ns, summary->p99_ns,
            summary->p999_ns, summary->max_ns);
    if (fclose(meta) != 0) {
        int saved_errno = errno == 0 ? EIO : errno;
        free(meta_path);
        return saved_errno;
    }

    free(meta_path);
    return 0;
}

static int ramp_latency_run(const ramp_latency_config *config, double calibration_qps,
                            void *context, ramp_latency_query_fn query,
                            ramp_latency_summary *summary) {
    ramp_latency_shared shared;
    pthread_t producer;
    uint64_t query_id = 0;
    uint64_t local_service_ns = 0;
    int status;

    if (config == NULL || query == NULL || summary == NULL || config->csv_path == NULL ||
        config->max_queries == 0 || calibration_qps <= 0.0 || !isfinite(calibration_qps))
        return EINVAL;
    if (config->max_queries > SIZE_MAX / sizeof(ramp_latency_record))
        return EOVERFLOW;

    memset(&shared, 0, sizeof(shared));
    shared.records =
        (ramp_latency_record *)malloc((size_t)config->max_queries * sizeof(*shared.records));
    if (shared.records == NULL)
        return ENOMEM;

    /* Write every page before timing begins so first-touch faults stay out. */
    for (query_id = 0; query_id < config->max_queries; ++query_id) {
        shared.records[query_id].incoming_ns = UINT64_MAX;
        shared.records[query_id].responded_ns = UINT64_MAX;
    }

    shared.config = config;
    shared.calibration_qps = calibration_qps;
    shared.start_ns = ramp_latency_now_ns() + UINT64_C(10000000);
    query_id = 0;

    status = pthread_create(&producer, NULL, ramp_latency_producer, &shared);
    if (status != 0) {
        free(shared.records);
        return status;
    }

    for (;;) {
        uint64_t produced = __atomic_load_n(&shared.produced, __ATOMIC_ACQUIRE);
        if (query_id < produced) {
            uint64_t service_start_ns = ramp_latency_now_ns();
            int query_status = query(context, query_id);
            uint64_t response_absolute_ns = ramp_latency_now_ns();
            local_service_ns += response_absolute_ns - service_start_ns;
            shared.records[query_id].responded_ns = response_absolute_ns - shared.start_ns;
            ++query_id;

            if ((query_id & 63) == 0 || query_status != 0) {
                __atomic_store_n(&shared.service_ns, local_service_ns, __ATOMIC_RELEASE);
                __atomic_store_n(&shared.responded, query_id, __ATOMIC_RELEASE);
            }
            if (query_status != 0) {
                __atomic_store_n(&shared.query_error, query_status, __ATOMIC_RELEASE);
            }
        } else if (__atomic_load_n(&shared.producer_done, __ATOMIC_ACQUIRE) != 0) {
            break;
        } else {
            ramp_latency_cpu_relax();
        }
    }

    __atomic_store_n(&shared.service_ns, local_service_ns, __ATOMIC_RELEASE);
    __atomic_store_n(&shared.responded, query_id, __ATOMIC_RELEASE);
    pthread_join(producer, NULL);

    status = __atomic_load_n(&shared.query_error, __ATOMIC_ACQUIRE);
    if (status == 0)
        status = ramp_latency_write_results(&shared, summary);
    free(shared.records);
    return status;
}

static void ramp_latency_print_summary(const ramp_latency_config *config,
                                       const ramp_latency_summary *summary) {
    fprintf(stderr,
            "RAMP_LATENCY implementation=%s N=%" PRIu64 " queries=%" PRIu64
            " stop=%s calibration_qps=%.2f"
            " final_input_qps=%.2f avg_us=%.3f p50_us=%.3f p95_us=%.3f"
            " p99_us=%.3f p999_us=%.3f max_us=%.3f raw=%s\n",
            config->implementation, config->map_size, summary->queries, summary->stop_reason,
            summary->calibration_qps, summary->final_target_qps,
            summary->average_latency_ns / 1000.0, summary->p50_ns / 1000.0,
            summary->p95_ns / 1000.0, summary->p99_ns / 1000.0, summary->p999_ns / 1000.0,
            summary->max_ns / 1000.0, config->csv_path);
}

#ifdef __cplusplus
}
#endif

#endif
