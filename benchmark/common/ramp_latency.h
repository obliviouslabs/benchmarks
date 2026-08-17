#ifndef OBLIVIOUS_BENCHMARK_RAMP_LATENCY_H
#define OBLIVIOUS_BENCHMARK_RAMP_LATENCY_H

/*
 * Open-loop latency benchmark for synchronous map implementations.
 *
 * The input-generator thread only controls when requests enter the queue. The
 * calling thread independently drains that queue, either one request at a
 * time or by passing all currently pending requests (up to a caller-specific
 * maximum) to a batch callback. Queueing delay is therefore included in the
 * incoming-to-responded latency.
 *
 * The hot path only reads the monotonic clock and writes pre-faulted memory.
 * CSV and metadata files are opened after the generator has stopped and the
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
typedef int (*ramp_latency_batch_query_fn)(void *context, uint64_t first_query_id,
                                           uint64_t query_count);

typedef enum ramp_latency_workload {
    RAMP_LATENCY_WORKLOAD_RAMP = 0,
    RAMP_LATENCY_WORKLOAD_CONSTANT = 1,
    RAMP_LATENCY_WORKLOAD_INTERMITTENT = 2
} ramp_latency_workload;

typedef struct ramp_latency_record {
    uint64_t incoming_ns;
    uint64_t responded_ns;
    uint64_t caller_batch_size;
} ramp_latency_record;

typedef struct ramp_latency_config {
    uint64_t map_size;
    uint64_t min_queries;
    uint64_t max_queries;
    uint64_t phase_queries;
    uint64_t calibration_queries;
    uint64_t caller_max_batch_size;
    uint64_t intermittent_batch_size;
    uint32_t overload_phases;
    double start_fraction;
    double step_fraction;
    double constant_qps;
    double constant_fraction;
    double intermittent_wait_multiplier;
    ramp_latency_workload workload;
    int check_output;
    const char *implementation;
    const char *csv_path;
} ramp_latency_config;

typedef struct ramp_latency_summary {
    uint64_t queries;
    uint64_t elapsed_ns;
    uint64_t caller_batches;
    uint64_t maximum_caller_batch_size;
    double calibration_qps;
    double final_target_qps;
    double average_caller_batch_size;
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

static uint64_t ramp_latency_saturating_add(uint64_t a, uint64_t b) {
    if (b > UINT64_MAX - a)
        return UINT64_MAX;
    return a + b;
}

static const char *ramp_latency_workload_name(ramp_latency_workload workload) {
    switch (workload) {
    case RAMP_LATENCY_WORKLOAD_CONSTANT:
        return "constant";
    case RAMP_LATENCY_WORKLOAD_INTERMITTENT:
        return "intermittent";
    case RAMP_LATENCY_WORKLOAD_RAMP:
    default:
        return "ramp";
    }
}

static ramp_latency_workload ramp_latency_workload_from_env(void) {
    const char *raw = getenv("RAMP_WORKLOAD");
    if (raw == NULL || raw[0] == '\0' || strcmp(raw, "ramp") == 0)
        return RAMP_LATENCY_WORKLOAD_RAMP;
    if (strcmp(raw, "constant") == 0)
        return RAMP_LATENCY_WORKLOAD_CONSTANT;
    if (strcmp(raw, "intermittent") == 0)
        return RAMP_LATENCY_WORKLOAD_INTERMITTENT;
    fprintf(stderr, "unknown RAMP_WORKLOAD=%s; using ramp\n", raw);
    return RAMP_LATENCY_WORKLOAD_RAMP;
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
    config->caller_max_batch_size = 1;
    config->intermittent_batch_size =
        ramp_latency_env_u64("RAMP_INTERMITTENT_BATCH_SIZE", 1024);
    if (config->intermittent_batch_size == 0)
        config->intermittent_batch_size = 1;
    config->overload_phases = (uint32_t)ramp_latency_env_u64("RAMP_OVERLOAD_PHASES", 2);
    if (config->overload_phases == 0)
        config->overload_phases = 1;
    config->start_fraction = ramp_latency_env_double("RAMP_START_FRACTION", 0.25);
    config->step_fraction = ramp_latency_env_double("RAMP_STEP_FRACTION", 0.025);
    config->constant_qps = ramp_latency_env_double("RAMP_CONSTANT_QPS", 0.0);
    config->constant_fraction = ramp_latency_env_double("RAMP_CONSTANT_FRACTION", 1.0);
    config->intermittent_wait_multiplier =
        ramp_latency_env_double("RAMP_INTERMITTENT_WAIT_MULTIPLIER", 3.0);
    if (config->start_fraction <= 0.0)
        config->start_fraction = 0.25;
    if (config->step_fraction <= 0.0)
        config->step_fraction = 0.025;
    if (config->constant_qps < 0.0)
        config->constant_qps = 0.0;
    if (config->constant_fraction <= 0.0)
        config->constant_fraction = 1.0;
    if (config->intermittent_wait_multiplier < 0.0)
        config->intermittent_wait_multiplier = 3.0;
    config->workload = ramp_latency_workload_from_env();
    config->check_output = 0;
    config->implementation = implementation;
    config->csv_path = csv_path;
}

static void ramp_latency_enable_batching(ramp_latency_config *config,
                                         uint64_t default_max_batch_size) {
    if (default_max_batch_size == 0)
        default_max_batch_size = 1;
    config->caller_max_batch_size =
        ramp_latency_env_u64("RAMP_MAX_BATCH_SIZE", default_max_batch_size);
    if (config->caller_max_batch_size == 0)
        config->caller_max_batch_size = 1;
}

typedef struct ramp_latency_single_query_adapter {
    void *context;
    ramp_latency_query_fn query;
} ramp_latency_single_query_adapter;

static int ramp_latency_call_single_as_batch(void *raw_adapter, uint64_t first_query_id,
                                             uint64_t query_count) {
    ramp_latency_single_query_adapter *adapter =
        (ramp_latency_single_query_adapter *)raw_adapter;
    uint64_t i;
    for (i = 0; i < query_count; ++i) {
        int status = adapter->query(adapter->context, first_query_id + i);
        if (status != 0)
            return status;
    }
    return 0;
}

static int ramp_latency_calibrate_batch(void *context, ramp_latency_batch_query_fn query_batch,
                                        uint64_t query_count, uint64_t max_batch_size,
                                        double *qps_out) {
    uint64_t pass;
    uint64_t start_ns = 0;
    uint64_t elapsed_ns;

    if (query_batch == NULL || qps_out == NULL || query_count == 0 || max_batch_size == 0)
        return EINVAL;

    /* Warm once, then time an identical pass with the same batch boundaries. */
    for (pass = 0; pass < 2; ++pass) {
        uint64_t first_query_id = 0;
        if (pass == 1)
            start_ns = ramp_latency_now_ns();
        while (first_query_id < query_count) {
            uint64_t count = query_count - first_query_id;
            int status;
            if (count > max_batch_size)
                count = max_batch_size;
            status = query_batch(context, first_query_id, count);
            if (status != 0)
                return status;
            first_query_id += count;
        }
    }

    elapsed_ns = ramp_latency_now_ns() - start_ns;
    if (elapsed_ns == 0)
        elapsed_ns = 1;
    *qps_out = (double)query_count * 1e9 / (double)elapsed_ns;
    return 0;
}

static int ramp_latency_calibrate(void *context, ramp_latency_query_fn query, uint64_t query_count,
                                  double *qps_out) {
    ramp_latency_single_query_adapter adapter;
    if (query == NULL)
        return EINVAL;
    adapter.context = context;
    adapter.query = query;
    return ramp_latency_calibrate_batch(&adapter, ramp_latency_call_single_as_batch, query_count, 1,
                                        qps_out);
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

static int ramp_latency_generator_failed(ramp_latency_shared *shared) {
    return __atomic_load_n(&shared->query_error, __ATOMIC_ACQUIRE) != 0;
}

static void ramp_latency_publish_one(ramp_latency_shared *shared, uint64_t query_id) {
    shared->records[query_id].incoming_ns = ramp_latency_now_ns() - shared->start_ns;
    __atomic_store_n(&shared->produced, query_id + 1, __ATOMIC_RELEASE);
}

static void ramp_latency_generate_ramp(ramp_latency_shared *shared) {
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
        ramp_latency_wait_until(shared->start_ns + (uint64_t)scheduled_offset_ns);
        if (ramp_latency_generator_failed(shared)) {
            shared->stop_reason = "query_error";
            break;
        }
        ramp_latency_publish_one(shared, query_id);
        ++query_id;
    }

    shared->final_queries = query_id;
    shared->final_target_qps = target_qps;
    if (query_id == config->max_queries && shared->stop_reason == NULL)
        shared->stop_reason = "max_queries";
}

static void ramp_latency_generate_constant(ramp_latency_shared *shared) {
    const ramp_latency_config *config = shared->config;
    long double scheduled_offset_ns = 0.0L;
    uint64_t query_id = 0;
    double target_qps = config->constant_qps > 0.0
                            ? config->constant_qps
                            : shared->calibration_qps * config->constant_fraction;

    while (query_id < config->max_queries) {
        scheduled_offset_ns += 1e9L / (long double)target_qps;
        ramp_latency_wait_until(shared->start_ns + (uint64_t)scheduled_offset_ns);
        if (ramp_latency_generator_failed(shared)) {
            shared->stop_reason = "query_error";
            break;
        }
        ramp_latency_publish_one(shared, query_id);
        ++query_id;
    }

    shared->final_queries = query_id;
    shared->final_target_qps = target_qps;
    if (query_id == config->max_queries && shared->stop_reason == NULL)
        shared->stop_reason = "max_queries";
}

static void ramp_latency_generate_intermittent(ramp_latency_shared *shared) {
    const ramp_latency_config *config = shared->config;
    uint64_t query_id = 0;

    ramp_latency_wait_until(shared->start_ns);
    while (query_id < config->max_queries) {
        uint64_t burst_size = config->max_queries - query_id;
        uint64_t dispatch_absolute_ns;
        uint64_t dispatch_ns;
        uint64_t burst_end;
        uint64_t response_duration_ns;
        uint64_t idle_ns;
        uint64_t i;

        if (burst_size > config->intermittent_batch_size)
            burst_size = config->intermittent_batch_size;
        dispatch_absolute_ns = ramp_latency_now_ns();
        dispatch_ns = dispatch_absolute_ns - shared->start_ns;
        burst_end = query_id + burst_size;
        for (i = query_id; i < burst_end; ++i)
            shared->records[i].incoming_ns = dispatch_ns;
        __atomic_store_n(&shared->produced, burst_end, __ATOMIC_RELEASE);

        while (__atomic_load_n(&shared->responded, __ATOMIC_ACQUIRE) < burst_end &&
               !ramp_latency_generator_failed(shared))
            ramp_latency_cpu_relax();
        if (ramp_latency_generator_failed(shared)) {
            shared->stop_reason = "query_error";
            break;
        }

        query_id = burst_end;
        if (query_id == config->max_queries)
            break;
        response_duration_ns = shared->records[burst_end - 1].responded_ns - dispatch_ns;
        if ((long double)response_duration_ns * config->intermittent_wait_multiplier >=
            (long double)UINT64_MAX) {
            idle_ns = UINT64_MAX;
        } else {
            idle_ns = (uint64_t)((long double)response_duration_ns *
                                 config->intermittent_wait_multiplier);
        }
        ramp_latency_wait_until(
            ramp_latency_saturating_add(ramp_latency_now_ns(), idle_ns));
    }

    shared->final_queries = query_id;
    shared->final_target_qps = 0.0;
    if (query_id == config->max_queries && shared->stop_reason == NULL)
        shared->stop_reason = "max_queries";
}

static void *ramp_latency_input_generator(void *raw_shared) {
    ramp_latency_shared *shared = (ramp_latency_shared *)raw_shared;
    switch (shared->config->workload) {
    case RAMP_LATENCY_WORKLOAD_CONSTANT:
        ramp_latency_generate_constant(shared);
        break;
    case RAMP_LATENCY_WORKLOAD_INTERMITTENT:
        ramp_latency_generate_intermittent(shared);
        break;
    case RAMP_LATENCY_WORKLOAD_RAMP:
    default:
        ramp_latency_generate_ramp(shared);
        break;
    }
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

static int ramp_latency_write_results(ramp_latency_shared *shared, ramp_latency_summary *summary,
                                      uint64_t caller_batches,
                                      uint64_t maximum_caller_batch_size) {
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
    summary->caller_batches = caller_batches;
    summary->maximum_caller_batch_size = maximum_caller_batch_size;
    summary->average_caller_batch_size =
        caller_batches == 0 ? 0.0 : (double)count / (double)caller_batches;
    summary->calibration_qps = shared->calibration_qps;
    summary->final_target_qps = shared->final_target_qps;
    summary->stop_reason = shared->stop_reason == NULL ? "unknown" : shared->stop_reason;
    if (count != 0)
        summary->elapsed_ns = shared->records[count - 1].responded_ns;

    csv = fopen(config->csv_path, "w");
    if (csv == NULL)
        return errno == 0 ? EIO : errno;
    fprintf(csv, "query_id,incoming_ns,responded_ns,caller_batch_size\n");
    for (i = 0; i < count; ++i) {
        fprintf(csv, "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", i,
                shared->records[i].incoming_ns, shared->records[i].responded_ns,
                shared->records[i].caller_batch_size);
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
            "  \"schema_version\": 2,\n"
            "  \"implementation\": \"%s\",\n"
            "  \"workload\": \"%s\",\n"
            "  \"check_output\": %s,\n"
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
            "  \"constant_qps\": %.9f,\n"
            "  \"constant_fraction\": %.9f,\n"
            "  \"intermittent_batch_size\": %" PRIu64 ",\n"
            "  \"intermittent_wait_multiplier\": %.9f,\n"
            "  \"caller_max_batch_size\": %" PRIu64 ",\n"
            "  \"caller_batches\": %" PRIu64 ",\n"
            "  \"average_caller_batch_size\": %.9f,\n"
            "  \"maximum_caller_batch_size\": %" PRIu64 ",\n"
            "  \"final_target_qps\": %.9f,\n"
            "  \"stop_reason\": \"%s\",\n"
            "  \"average_latency_ns\": %.3f,\n"
            "  \"p50_latency_ns\": %" PRIu64 ",\n"
            "  \"p95_latency_ns\": %" PRIu64 ",\n"
            "  \"p99_latency_ns\": %" PRIu64 ",\n"
            "  \"p999_latency_ns\": %" PRIu64 ",\n"
            "  \"max_latency_ns\": %" PRIu64 "\n"
            "}\n",
            config->implementation, ramp_latency_workload_name(config->workload),
            config->check_output ? "true" : "false", config->map_size, summary->queries,
            config->min_queries, config->max_queries, config->phase_queries,
            config->calibration_queries, config->calibration_queries, summary->calibration_qps,
            config->start_fraction, config->step_fraction, config->constant_qps,
            config->constant_fraction, config->intermittent_batch_size,
            config->intermittent_wait_multiplier, config->caller_max_batch_size,
            summary->caller_batches, summary->average_caller_batch_size,
            summary->maximum_caller_batch_size, summary->final_target_qps, summary->stop_reason,
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

static int ramp_latency_run_batch(const ramp_latency_config *config, double calibration_qps,
                                  void *context, ramp_latency_batch_query_fn query_batch,
                                  ramp_latency_summary *summary) {
    ramp_latency_shared shared;
    pthread_t producer;
    uint64_t query_id = 0;
    uint64_t local_service_ns = 0;
    uint64_t caller_batches = 0;
    uint64_t maximum_caller_batch_size = 0;
    int status;

    if (config == NULL || query_batch == NULL || summary == NULL || config->csv_path == NULL ||
        config->max_queries == 0 || config->caller_max_batch_size == 0 ||
        calibration_qps <= 0.0 || !isfinite(calibration_qps))
        return EINVAL;
    if (config->max_queries > SIZE_MAX / sizeof(ramp_latency_record))
        return EOVERFLOW;

    memset(&shared, 0, sizeof(shared));
    shared.records =
        (ramp_latency_record *)malloc((size_t)config->max_queries * sizeof(*shared.records));
    if (shared.records == NULL)
        return ENOMEM;

    for (query_id = 0; query_id < config->max_queries; ++query_id) {
        shared.records[query_id].incoming_ns = UINT64_MAX;
        shared.records[query_id].responded_ns = UINT64_MAX;
        shared.records[query_id].caller_batch_size = 0;
    }

    shared.config = config;
    shared.calibration_qps = calibration_qps;
    shared.start_ns = ramp_latency_now_ns() + UINT64_C(10000000);
    query_id = 0;

    status = pthread_create(&producer, NULL, ramp_latency_input_generator, &shared);
    if (status != 0) {
        free(shared.records);
        return status;
    }

    for (;;) {
        uint64_t produced = __atomic_load_n(&shared.produced, __ATOMIC_ACQUIRE);
        if (query_id < produced) {
            uint64_t batch_size = produced - query_id;
            uint64_t service_start_ns;
            uint64_t response_absolute_ns;
            uint64_t response_ns;
            uint64_t i;
            int query_status;
            if (batch_size > config->caller_max_batch_size)
                batch_size = config->caller_max_batch_size;

            service_start_ns = ramp_latency_now_ns();
            query_status = query_batch(context, query_id, batch_size);
            response_absolute_ns = ramp_latency_now_ns();
            response_ns = response_absolute_ns - shared.start_ns;
            local_service_ns += response_absolute_ns - service_start_ns;
            for (i = query_id; i < query_id + batch_size; ++i) {
                shared.records[i].responded_ns = response_ns;
                shared.records[i].caller_batch_size = batch_size;
            }
            query_id += batch_size;
            ++caller_batches;
            if (batch_size > maximum_caller_batch_size)
                maximum_caller_batch_size = batch_size;

            if (query_status != 0)
                __atomic_store_n(&shared.query_error, query_status, __ATOMIC_RELEASE);
            if (config->caller_max_batch_size > 1 || query_status != 0 ||
                (query_id & 63) == 0 ||
                (config->workload == RAMP_LATENCY_WORKLOAD_INTERMITTENT &&
                 query_id == produced)) {
                __atomic_store_n(&shared.service_ns, local_service_ns, __ATOMIC_RELEASE);
                __atomic_store_n(&shared.responded, query_id, __ATOMIC_RELEASE);
            }
            if (query_status != 0) {
                break;
            }
        /* Reload produced after observing done; the final publish may have
         * raced with the snapshot at the top of this iteration. */
        } else if (__atomic_load_n(&shared.producer_done, __ATOMIC_ACQUIRE) != 0 &&
                   query_id >= __atomic_load_n(&shared.produced, __ATOMIC_ACQUIRE)) {
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
        status = ramp_latency_write_results(&shared, summary, caller_batches,
                                            maximum_caller_batch_size);
    free(shared.records);
    return status;
}

static int ramp_latency_run(const ramp_latency_config *config, double calibration_qps,
                            void *context, ramp_latency_query_fn query,
                            ramp_latency_summary *summary) {
    ramp_latency_single_query_adapter adapter;
    ramp_latency_config scalar_config;
    if (config == NULL || query == NULL)
        return EINVAL;
    adapter.context = context;
    adapter.query = query;
    scalar_config = *config;
    scalar_config.caller_max_batch_size = 1;
    return ramp_latency_run_batch(&scalar_config, calibration_qps, &adapter,
                                  ramp_latency_call_single_as_batch, summary);
}

static void ramp_latency_print_summary(const ramp_latency_config *config,
                                       const ramp_latency_summary *summary) {
    fprintf(stderr,
            "RAMP_LATENCY implementation=%s workload=%s N=%" PRIu64 " queries=%" PRIu64
            " stop=%s calibration_qps=%.2f final_input_qps=%.2f"
            " caller_batches=%" PRIu64 " avg_batch=%.2f max_batch=%" PRIu64
            " avg_us=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f"
            " p999_us=%.3f max_us=%.3f raw=%s\n",
            config->implementation, ramp_latency_workload_name(config->workload),
            config->map_size, summary->queries, summary->stop_reason, summary->calibration_qps,
            summary->final_target_qps, summary->caller_batches,
            summary->average_caller_batch_size, summary->maximum_caller_batch_size,
            summary->average_latency_ns / 1000.0, summary->p50_ns / 1000.0,
            summary->p95_ns / 1000.0, summary->p99_ns / 1000.0,
            summary->p999_ns / 1000.0, summary->max_ns / 1000.0, config->csv_path);
}

#ifdef __cplusplus
}
#endif

#endif
