/*
 * bench_opts_naive.c — same algorithms as bench_opts.mpy, written as
 * idiomatic C without micropy's automatic optimizations.
 *
 * Compiled alongside the micropy output to show what the compiler gains you:
 *   - no restrict  (compiler must assume pointer aliasing)
 *   - no constant specialization  (stride is a runtime variable)
 *   - malloc/free per call  (no alloca substitution)
 *   - no hot/cold outlining  (cold error paths stay in the hot instruction stream)
 *   - no prefetch insertion  (pointer-chase stalls on every L3 miss)
 *
 * Compile: cc -O2 -march=native -o bench_opts_naive bench_opts_naive.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define N           4000000
#define REPS        100
#define STRIDED_N   1000000
#define ALLOC_N     64
#define ALLOC_BYTES 512
#define ALLOC_REPS  5000000
#define SOA_N        5000000
#define SOA_REPS     20
#define RESTRICT_N   64
#define RESTRICT_REPS 50000000
#define HOT_N        256
#define HOT_REPS     5000000
#define LIST_N       1500007
#define LIST_STEP    750013    /* ~N/2, prime → jumps of ~12 MB defeat hardware prefetcher */
#define LIST_REPS    20

/* AoS layout: x/y/z/mass interleaved — accessing .x reads 32B but uses 8B */
typedef struct { double x, y, z, vx, vy, vz, mass, charge; } Particle;
static Particle particles[SOA_N];

static int64_t ms_now(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* No restrict — compiler must add a runtime aliasing-check prelude or
   fall back to scalar code to handle the case where out aliases x or y. */
static void saxpy(double *out, double *x, double *y, double a, int64_t n) {
    for (int64_t i = 0; i < n; i++)
        out[i] = a * x[i] + y[i];
}

/* stride is a runtime variable — compiler cannot fold it as a constant,
   so the index multiply stays in the loop and vectorisation is harder. */
static double strided_sum(double *arr, int64_t n, int64_t stride) {
    double total = 0.0;
    for (int64_t i = 0; i < n; i++)
        total += arr[i * stride];
    return total;
}

/* AoS field extract — copy .x into output; each load touches 64B but uses 8B */
static __attribute__((noinline)) void extract_x_aos(double *out, int64_t n) {
    for (int64_t i = 0; i < n; i++)
        out[i] = particles[i].x;
}

/* No restrict — 4 ptr params, compiler adds overlap-check preamble on each call */
static __attribute__((noinline)) void fused_mad_naive(double *out, double *a, double *b, double *c, int64_t n) {
    for (int64_t i = 0; i < n; i++)
        out[i] = a[i] * b[i] + c[i];
}

/* Cold error paths inline — bloats the hot instruction stream */
static __attribute__((noinline)) int64_t validated_sum_naive(int64_t *data, int64_t n) {
    if (data == NULL) {
        fprintf(stderr, "validated_sum: data pointer is NULL — caller must supply a non-null array\n");
        abort();
    }
    if (n <= 0) {
        fprintf(stderr, "validated_sum: n must be positive — received a zero or negative length\n");
        abort();
    }
    if (n > 10000000) {
        fprintf(stderr, "validated_sum: n exceeds maximum allowed size — possible buffer overflow\n");
        abort();
    }
    int64_t total = 0;
    for (int64_t i = 0; i < n; i++)
        total += data[i];
    return total;
}

/* Linked-list node — same stride-permutation layout as micropy version */
typedef struct LLNode { int64_t value; struct LLNode *next; } LLNode;
static LLNode ll_nodes[LIST_N];  /* 1.5M × 16B = 24 MB — scattered ~12 MB hops = L3/DRAM misses */

/* No prefetch — stalls on every pointer-chase cache miss */
static __attribute__((noinline)) int64_t walk_list_naive(LLNode *head) {
    int64_t total = 0;
    while (head != NULL) {
        total += head->value;
        head = head->next;
    }
    return total;
}

/* malloc/free per call — heap allocation cost paid every invocation.
   micropy replaces this with alloca(512) + no free (stack allocation). */
static int64_t process_with_scratch(int64_t *data, int64_t n) {
    int64_t *scratch = malloc(ALLOC_BYTES);
    for (int64_t i = 0; i < n; i++)
        scratch[i] = data[i] * 2 + 1;
    int64_t total = 0;
    for (int64_t i = 0; i < n; i++)
        total += scratch[i];
    free(scratch);
    return total;
}

int main(void) {
    double  *out  = malloc(N      * sizeof(double));
    double  *soa_out = malloc(SOA_N  * sizeof(double));
    double  *x    = malloc(N      * sizeof(double));
    double  *y    = malloc(N      * sizeof(double));
    int64_t *data = malloc(ALLOC_N * sizeof(int64_t));

    for (int64_t i = 0; i < N; i++) {
        x[i] = i * 0.001;
        y[i] = i * 0.002;
    }
    for (int64_t i = 0; i < ALLOC_N; i++)
        data[i] = i + 1;

    printf("%-20s  %12s  %8s\n", "benchmark", "result", "ms");
    printf("%-20s  %12s  %8s\n", "--------------------", "------------", "--------");

    int64_t t0 = ms_now();
    for (int r = 0; r < REPS; r++)
        saxpy(out, x, y, 2.5, N);
    int64_t t1 = ms_now();
    printf("%-20s  %12.6f  %8lld\n", "saxpy", out[N / 2], (long long)(t1 - t0));

    double result = 0.0;
    int64_t t2 = ms_now();
    for (int r = 0; r < REPS; r++)
        result = strided_sum(x, STRIDED_N, 4);
    int64_t t3 = ms_now();
    printf("%-20s  %12.6f  %8lld\n", "strided_sum", result, (long long)(t3 - t2));

    int64_t total = 0;
    int64_t t4 = ms_now();
    for (int r = 0; r < ALLOC_REPS; r++) {
        data[0] = r;     /* vary input to prevent loop-invariant hoisting */
        total += process_with_scratch(data, ALLOC_N);
    }
    int64_t t5 = ms_now();
    printf("%-20s  %12lld  %8lld\n", "small_alloc", (long long)(total % 1000000), (long long)(t5 - t4));

    for (int64_t i = 0; i < SOA_N; i++) {
        particles[i].x    = i * 0.001;
        particles[i].y    = i * 0.002;

    }
    int64_t t6 = ms_now();
    for (int r = 0; r < SOA_REPS; r++)
        extract_x_aos(soa_out, SOA_N);
    int64_t t7 = ms_now();
    printf("%-20s  %12.6f  %8lld\n", "soa_sum", soa_out[SOA_N / 2], (long long)(t7 - t6));

    double  *rm_out = malloc(RESTRICT_N * sizeof(double));
    double  *rm_a   = malloc(RESTRICT_N * sizeof(double));
    double  *rm_b   = malloc(RESTRICT_N * sizeof(double));
    double  *rm_c   = malloc(RESTRICT_N * sizeof(double));
    for (int64_t i = 0; i < RESTRICT_N; i++) {
        rm_a[i] = i * 0.1; rm_b[i] = i * 0.2; rm_c[i] = i * 0.05;
    }
    int64_t t8 = ms_now();
    for (int r = 0; r < RESTRICT_REPS; r++)
        fused_mad_naive(rm_out, rm_a, rm_b, rm_c, RESTRICT_N);
    int64_t t9 = ms_now();
    printf("%-20s  %12.6f  %8lld\n", "restrict_short", rm_out[RESTRICT_N / 2], (long long)(t9 - t8));

    int64_t *hc_data = malloc(HOT_N * sizeof(int64_t));
    for (int64_t i = 0; i < HOT_N; i++) hc_data[i] = i + 1;
    int64_t hc_total = 0;
    int64_t t10 = ms_now();
    for (int r = 0; r < HOT_REPS; r++) {
        hc_data[0] = r % 127 + 1;   /* vary input to prevent loop-invariant hoisting */
        hc_total += validated_sum_naive(hc_data, HOT_N);
    }
    int64_t t11 = ms_now();
    printf("%-20s  %12lld  %8lld\n", "hot_cold", (long long)(hc_total % 1000000), (long long)(t11 - t10));

    int64_t ll_idx = 0;
    for (int64_t i = 0; i < LIST_N - 1; i++) {
        int64_t ll_next = (ll_idx + LIST_STEP) % LIST_N;
        ll_nodes[ll_idx].value = ll_idx % 100;
        ll_nodes[ll_idx].next  = &ll_nodes[ll_next];
        ll_idx = ll_next;
    }
    ll_nodes[ll_idx].value = ll_idx % 100;
    ll_nodes[ll_idx].next  = NULL;
    int64_t ll_result = 0;
    int64_t t12 = ms_now();
    for (int r = 0; r < LIST_REPS; r++) {
        ll_nodes[0].value = r;   /* vary input to prevent loop-invariant hoisting */
        ll_result = walk_list_naive(&ll_nodes[0]);
    }
    int64_t t13 = ms_now();
    printf("%-20s  %12lld  %8lld\n", "linked_list", (long long)(ll_result % 1000000), (long long)(t13 - t12));

    free(out); free(x); free(y); free(data); free(soa_out);
    free(rm_out); free(rm_a); free(rm_b); free(rm_c); free(hc_data);
    return 0;
}