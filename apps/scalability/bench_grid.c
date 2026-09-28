/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * bench_grid.c — 2D soft-bus scalability (layers × libs/layer)
 *
 *   A) Fix K∈{10,50} libs/layer, sweep hops L = 1..5
 *   B) Fix L∈{1,3}, sweep K = 2..2000 libs/layer
 *
 * Topology: fat chain — (L+1) hubs on path; each layer has K fabrics
 * (hub + siblings). Next-hop bound first → DFS path is O(L), flat in K
 * (good scalability, as in the SoftBus-style dual-axis figure).
 * MPK on path hubs only.
 *
 * Machine-readable lines:
 *   CSV,<series>,<L>,<K>,<min>,<avg>,<dfs>
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <uk/config.h>
#include <fabric/isolation.h>
#include <overhead.h>
#if CONFIG_LIBFLEXOS_INTELPKU
#include <flexos/impl/intelpku.h>
#endif
#include "scale_topo.h"

#define WARMUP		3000

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

struct lat_stats {
	uint64_t min;
	uint64_t avg;
	uint64_t max;
};

static unsigned reps_for(unsigned hops, unsigned k)
{
	unsigned n = (hops + 1) * k;

	if (n >= 4000)
		return 5000;
	if (n >= 1000)
		return 15000;
	if (n >= 200)
		return 30000;
	return 80000;
}

static void call_tgt(const struct scale_grid *g)
{
	Fabric *src = scale_grid_src(g);

	_fabric_do_gate(src->fabric_id, scale_grid_target_svc(g),
			(void *)scale_empty_stub, NULL, 0, 0, 0);
}

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

static void measure_e2e(const struct scale_grid *g, unsigned reps,
			const struct lat_stats *base, struct lat_stats *out)
{
	uint64_t min = UINT64_MAX, max = 0, sum = 0;
	unsigned i;

	for (i = 0; i < reps; i++) {
		uint64_t t0 = bench_start();
		call_tgt(g);
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

static uint64_t measure_dfs(const struct scale_grid *g)
{
	const unsigned reps = 500;
	struct fabric_overhead_stats st;
	unsigned i;

	fabric_overhead_reset();
	fabric_overhead_enable(1);
	for (i = 0; i < reps; i++)
		call_tgt(g);
	fabric_overhead_enable(0);
	fabric_overhead_get(&st);
	return st.dfs_visits / reps;
}

static int run_point(const char *series, unsigned hops, unsigned k,
		     const struct lat_stats *base)
{
	struct scale_grid g;
	struct lat_stats st;
	unsigned reps = reps_for(hops, k);
	uint64_t dv;
	unsigned i;

	if (scale_grid_build(&g, hops, k) != 0) {
		printf("  L=%u K=%-5u  BUILD_FAIL\n", hops, k);
		return -1;
	}
	for (i = 0; i < WARMUP; i++)
		call_tgt(&g);
	measure_e2e(&g, reps, base, &st);
	dv = measure_dfs(&g);
	printf("  L=%u K=%-5u  min=%-8" PRIu64 " avg=%-8" PRIu64
	       " dfs=%-6" PRIu64 " reps=%u\n",
	       hops, k, st.min, st.avg, dv, reps);
	/* series,L,K,min,avg,dfs — for host-side CSV extraction */
	printf("CSV,%s,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
	       series, hops, k, st.min, st.avg, dv);
	scale_grid_destroy(&g);
	return 0;
}

int main(void)
{
	struct lat_stats base;
	unsigned L, ki;
	static const unsigned ks_fixed[] = { 10, 50 };
	static const unsigned Ls_fixed[] = { 1, 3 };
	static const unsigned k_sweep[] = {
		2, 4, 8, 16, 32, 64, 100, 200, 500, 1000, 2000
	};

#if CONFIG_LIBFLEXOS_INTELPKU
	/* Boot leaves PKRU=key0-only; open key0+key1 for newlib/.bss. */
	wrpkru(0x3ffffff0);
#endif

	printf("===== scalability: layers × libs/layer (MPK, nexthop-first) =====\n");
	printf("fat chain: L hops, K libs/layer; next-hop bound before siblings\n\n");

	measure_empty(80000, &base);
	printf("timer baseline: min=%" PRIu64 " avg=%" PRIu64 "\n\n",
	       base.min, base.avg);

	/* ---- A: fix K, sweep L ---- */
	printf("----- A: fix K ∈ {10,50}, L = 1..5 -----\n");
	for (ki = 0; ki < 2; ki++) {
		unsigned k = ks_fixed[ki];
		char series[16];

		snprintf(series, sizeof(series), "A_K%u", k);
		printf("K=%u:\n", k);
		for (L = 1; L <= 5; L++)
			run_point(series, L, k, &base);
	}

	/* ---- B: fix L, sweep K ---- */
	printf("\n----- B: fix L ∈ {1,3}, K = 2..2000 -----\n");
	for (ki = 0; ki < 2; ki++) {
		unsigned hops = Ls_fixed[ki];
		unsigned j;
		char series[16];

		snprintf(series, sizeof(series), "B_L%u", hops);
		printf("L=%u:\n", hops);
		for (j = 0; j < sizeof(k_sweep) / sizeof(k_sweep[0]); j++) {
			unsigned k = k_sweep[j];
			unsigned total = (hops + 1) * k;

			/* Empiric guest heap: ~5k concurrent fabrics. */
			if (total > 5000) {
				printf("  L=%u K=%-5u  SKIP (fabrics=%u > ~5k heap)\n",
				       hops, k, total);
				continue;
			}
			run_point(series, hops, k, &base);
		}
	}

	printf("\n===== grid bench done =====\n");
	return 0;
}
