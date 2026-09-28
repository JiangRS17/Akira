/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * bench_map.c — service mapping table size vs communication overhead
 *
 * Topology: 1-hop hub→leaf, capacity≈2·N, MPK on.
 *
 * For each N ∈ {8..512}:
 *   - fabric_gate to middle entry (N/2) and last entry (N-1)
 *   - phase: map_lookup avg
 *   - baseline: direct empty call (no fabric)
 *
 * CSV,e2e_mid,<N>,<cap>,<min>,<avg>,<map_avg>
 * CSV,e2e_last,<N>,<cap>,<min>,<avg>,<map_avg>
 * CSV,direct,<N>,0,<min>,<avg>,0
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

#define WARMUP		5000

static const unsigned map_sizes[] = {
	8, 16, 32, 64, 128, 256, 512
};
#define N_SIZES		((int)(sizeof(map_sizes) / sizeof(map_sizes[0])))

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

static unsigned reps_for(unsigned n)
{
	(void)n;
	return 80000;
}

static uint32_t round_up_pow2(uint32_t x)
{
	uint32_t p = 4;

	if (x <= 4)
		return 4;
	while (p < x)
		p <<= 1;
	return p;
}

static void call_named(Fabric *src, const char *name)
{
	_fabric_do_gate(src->fabric_id, name, (void *)scale_empty_stub,
			NULL, 0, 0, 0);
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

static void measure_direct(unsigned reps, const struct lat_stats *base,
			   struct lat_stats *out)
{
	uint64_t min = UINT64_MAX, max = 0, sum = 0;
	unsigned i;

	for (i = 0; i < reps; i++) {
		uint64_t t0 = bench_start();
		scale_empty_stub();
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

static void measure_e2e(Fabric *src, const char *name, unsigned reps,
			const struct lat_stats *base, struct lat_stats *out)
{
	uint64_t min = UINT64_MAX, max = 0, sum = 0;
	unsigned i;

	for (i = 0; i < reps; i++) {
		uint64_t t0 = bench_start();
		call_named(src, name);
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

static uint64_t measure_map_lookup_avg(Fabric *src, const char *name)
{
	const unsigned reps = 4000;
	struct fabric_overhead_stats st;
	unsigned i;

	fabric_overhead_reset();
	fabric_overhead_enable(1);
	for (i = 0; i < reps; i++)
		call_named(src, name);
	fabric_overhead_enable(0);
	fabric_overhead_get(&st);
	return st.map_lookup / reps;
}

static void emit_csv(const char *series, unsigned n, uint32_t cap,
		     const struct lat_stats *st, uint64_t map_avg)
{
	printf("CSV,%s,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
	       series, n, cap, st->min, st->avg, map_avg);
}

static int run_n(unsigned n_entries, const struct lat_stats *base)
{
	struct scale_star star;
	struct lat_stats st_mid, st_last, st_dir;
	char name_mid[16], name_last[16];
	unsigned mid = n_entries / 2;
	unsigned last = n_entries - 1;
	uint32_t cap = round_up_pow2(n_entries * 2);
	unsigned reps = reps_for(n_entries);
	uint64_t map_mid, map_last;
	unsigned i;

	fabric_create_skip_queues = 1;
	if (scale_star_build(&star, 2) != 0) {
		printf("  N=%u BUILD_FAIL(star)\n", n_entries);
		return -1;
	}
	if (scale_map_recreate(star.leaves[0], cap) != 0 ||
	    scale_map_fill(star.leaves[0], n_entries, mid, name_mid,
			   sizeof(name_mid)) != 0) {
		printf("  N=%u BUILD_FAIL(map)\n", n_entries);
		scale_star_destroy(&star);
		return -1;
	}
	snprintf(name_last, sizeof(name_last), "S%06u", last);

	for (i = 0; i < WARMUP; i++) {
		call_named(star.hub, name_mid);
		call_named(star.hub, name_last);
		scale_empty_stub();
	}

	measure_direct(reps, base, &st_dir);
	measure_e2e(star.hub, name_mid, reps, base, &st_mid);
	map_mid = measure_map_lookup_avg(star.hub, name_mid);
	measure_e2e(star.hub, name_last, reps, base, &st_last);
	map_last = measure_map_lookup_avg(star.hub, name_last);

	printf("  N=%-4u cap=%-4u  direct=%-4" PRIu64
	       "  mid=%-4" PRIu64 " (map=%" PRIu64 ")"
	       "  last=%-4" PRIu64 " (map=%" PRIu64 ")\n",
	       n_entries, cap, st_dir.avg, st_mid.avg, map_mid,
	       st_last.avg, map_last);

	emit_csv("direct", n_entries, 0, &st_dir, 0);
	emit_csv("e2e_mid", n_entries, cap, &st_mid, map_mid);
	emit_csv("e2e_last", n_entries, cap, &st_last, map_last);

	scale_star_destroy(&star);
	return 0;
}

int main(void)
{
	struct lat_stats base;
	int i;

#if CONFIG_LIBFLEXOS_INTELPKU
	wrpkru(0x3ffffff0);
#endif

	printf("===== scalability: service map (multi-metric) =====\n");
	printf("direct | e2e mid | e2e last | map_lookup phase\n\n");

	measure_empty(80000, &base);
	printf("timer baseline: min=%" PRIu64 " avg=%" PRIu64 "\n\n",
	       base.min, base.avg);

	for (i = 0; i < N_SIZES; i++)
		run_n(map_sizes[i], &base);

	printf("\n===== map bench done =====\n");
	return 0;
}
