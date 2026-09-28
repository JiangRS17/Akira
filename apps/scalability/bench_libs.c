/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * bench_libs.c — component-library count (N) vs communication latency
 *
 * Topology: star — hub + (N-1) leaves, fixed 1-hop to the *first* leaf.
 * Extra libraries are unrelated siblings; path cost should stay ~flat in N.
 *
 * Reporting: min + avg (+ max). Pure min often lands on the same floor and
 * looks "zero noise"; avg shows normal run-to-run jitter while staying flat.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <fabric/isolation.h>
#include <overhead.h>
#include "scale_topo.h"

#define WARMUP		5000

static const unsigned lib_counts[] = {
	2, 4, 8, 16, 32, 64, 100, 200, 500
};
#define N_POINTS	((int)(sizeof(lib_counts) / sizeof(lib_counts[0])))

static void scale_empty_stub(void)
{
	asm volatile("");
}

__attribute__((always_inline)) static inline uint64_t bench_start(void)
{
	unsigned lo, hi;

	asm volatile(
		"CPUID\n\tRDTSC\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\t"
		: "=r"(hi), "=r"(lo)
		:
		: "%rax", "%rbx", "%rcx", "%rdx");
	return ((uint64_t)hi << 32) | lo;
}

__attribute__((always_inline)) static inline uint64_t bench_end(void)
{
	unsigned lo, hi;

	asm volatile(
		"RDTSCP\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\tCPUID\n\t"
		: "=r"(hi), "=r"(lo)
		:
		: "%rax", "%rbx", "%rcx", "%rdx");
	return ((uint64_t)hi << 32) | lo;
}

static unsigned e2e_reps_for(unsigned n_libs)
{
	if (n_libs >= 200)
		return 20000;
	if (n_libs >= 64)
		return 50000;
	return 100000;
}

static void call_target(const struct scale_star *star)
{
	const char *name = scale_star_target_svc(star);

	_fabric_do_gate(star->hub->fabric_id, name, (void *)scale_empty_stub,
			NULL, 0, 0, 0);
}

struct lat_stats {
	uint64_t min;
	uint64_t max;
	uint64_t avg; /* integer mean */
};

static void measure_empty(unsigned reps, struct lat_stats *out)
{
	uint64_t min = UINT64_MAX, max = 0, sum = 0;
	unsigned i;

	for (i = 0; i < reps; i++) {
		uint64_t t0 = bench_start();
		asm volatile("");
		uint64_t t1 = bench_end();
		uint64_t d = t1 - t0;

		if (d < min)
			min = d;
		if (d > max)
			max = d;
		sum += d;
	}
	out->min = min;
	out->max = max;
	out->avg = sum / reps;
}

static void measure_e2e(const struct scale_star *star, unsigned reps,
			const struct lat_stats *base, struct lat_stats *out)
{
	uint64_t min = UINT64_MAX, max = 0, sum = 0;
	unsigned i;

	for (i = 0; i < reps; i++) {
		uint64_t t0 = bench_start();
		call_target(star);
		uint64_t t1 = bench_end();
		uint64_t d = t1 - t0;

		if (d < min)
			min = d;
		if (d > max)
			max = d;
		sum += d;
	}
	out->min = (min > base->min) ? (min - base->min) : 0;
	out->max = (max > base->min) ? (max - base->min) : 0;
	out->avg = (sum / reps > base->avg) ? (sum / reps - base->avg) : 0;
}

static uint64_t measure_dfs_visits(const struct scale_star *star)
{
	const unsigned reps = 1000;
	struct fabric_overhead_stats st;
	unsigned i;

	fabric_overhead_reset();
	fabric_overhead_enable(1);
	for (i = 0; i < reps; i++)
		call_target(star);
	fabric_overhead_enable(0);
	fabric_overhead_get(&st);
	return st.dfs_visits / reps;
}

int main(void)
{
	struct scale_star star;
	struct lat_stats base;
	int p;

	printf("===== scalability: component-library count =====\n");
	printf("star hub+(N-1) leaves; 1-hop to *first* leaf; no iso\n");
	printf("path is O(1) in N (dfs_visits=2); use avg — min alone looks flat\n\n");

	measure_empty(100000, &base);
	printf("timer baseline: min=%" PRIu64 " avg=%" PRIu64 " max=%" PRIu64 "\n\n",
	       base.min, base.avg, base.max);

	printf("%-6s %8s %8s %8s %10s %8s\n",
	       "N", "min", "avg", "max", "dfs_vis", "reps");

	for (p = 0; p < N_POINTS; p++) {
		unsigned n = lib_counts[p];
		unsigned reps = e2e_reps_for(n);
		struct lat_stats st;
		uint64_t dv;
		unsigned i;

		if (scale_star_build(&star, n) != 0) {
			printf("%-6u  BUILD_FAIL\n", n);
			continue;
		}

		for (i = 0; i < WARMUP; i++)
			call_target(&star);

		measure_e2e(&star, reps, &base, &st);
		dv = measure_dfs_visits(&star);

		printf("%-6u %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " %10" PRIu64 " %8u\n",
		       n, st.min, st.avg, st.max, dv, reps);

		scale_star_destroy(&star);
	}

	printf("\n===== libs bench done =====\n");
	return 0;
}
