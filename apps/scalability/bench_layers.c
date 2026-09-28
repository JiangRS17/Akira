/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * bench_layers.c — soft-bus layer (hop) count vs communication overhead
 *
 * Topology: bidirectional chain F0—F1—…—F[max_hops]
 * Call path: fabric_gate from F0 to service "Lxx" registered only on F[hops]
 *
 * Build:
 *   cd apps/scalability && make olddefconfig && make -j$(nproc)
 * Run (requires -cpu host for PKU passthrough):
 *   ./run.sh
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <fabric/isolation.h>
#include <overhead.h>
#include "scale_topo.h"

#define WARMUP		10000
#define E2E_REPS	500000
#define PROF_REPS	100000

/* Measure hops 1..8 on one chain (array sized by SCALE_MAX_HOPS). */
#define BENCH_MAX_HOPS	8

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

/*
 * Target is resolved by service_id from name (DFS + map).
 * direct_fn must be non-NULL; used under FABRIC_ABL_DIRECT_FN.
 */
static void scale_empty_stub(void)
{
	asm volatile("");
}

static void call_hops(const struct scale_chain *chain, unsigned hops)
{
	const char *name = scale_chain_svc(chain, hops);

	_fabric_do_gate(0, name, (void *)scale_empty_stub, NULL, 0, 0, 0);
}

static uint64_t measure_empty(void)
{
	uint64_t min = UINT64_MAX;
	int i;

	for (i = 0; i < E2E_REPS; i++) {
		uint64_t t0 = bench_start();
		asm volatile("");
		uint64_t t1 = bench_end();

		if ((t1 - t0) < min)
			min = t1 - t0;
	}
	return min;
}

static uint64_t measure_e2e_min(const struct scale_chain *chain,
				unsigned hops, uint64_t base)
{
	uint64_t min = UINT64_MAX;
	int i;

	for (i = 0; i < E2E_REPS; i++) {
		uint64_t t0 = bench_start();
		call_hops(chain, hops);
		uint64_t t1 = bench_end();

		if ((t1 - t0) < min)
			min = t1 - t0;
	}
	return min - base;
}

static void print_phase_row(const char *name, uint64_t v, uint64_t sum)
{
	uint64_t pct = sum ? (v * 100) / sum : 0;

	printf("  %-14s %8" PRIu64 "  %3" PRIu64 "%%\n", name, v, pct);
}

static void profile_hops(const struct scale_chain *chain, unsigned hops,
			 struct fabric_overhead_stats *out, uint64_t *e2e_out,
			 uint64_t base)
{
	int i;

	fabric_ablation_set(FABRIC_ABL_NONE);

	for (i = 0; i < WARMUP; i++)
		call_hops(chain, hops);

	*e2e_out = measure_e2e_min(chain, hops, base);

	fabric_overhead_reset();
	fabric_overhead_enable(1);
	for (i = 0; i < PROF_REPS; i++)
		call_hops(chain, hops);
	fabric_overhead_enable(0);
	fabric_overhead_get(out);
}

int main(void)
{
	struct scale_chain chain;
	uint64_t base;
	struct fabric_overhead_stats st[BENCH_MAX_HOPS];
	uint64_t e2e[BENCH_MAX_HOPS];
	unsigned h;

	printf("===== scalability: soft-bus layers =====\n");
	printf("chain hops=1..%u  (DFS from fabric 0 to Lxx on fabric hops)\n\n",
	       BENCH_MAX_HOPS);

	if (scale_chain_build(&chain, BENCH_MAX_HOPS) != 0) {
		printf("ERROR: scale_chain_build failed\n");
		return 1;
	}

	/* sanity: one call per hop */
	for (h = 1; h <= BENCH_MAX_HOPS; h++)
		call_hops(&chain, h);
	printf("sanity: hops 1..%u OK\n\n", BENCH_MAX_HOPS);

	base = measure_empty();
	printf("timer baseline: %" PRIu64 "\n\n", base);

	for (h = 1; h <= BENCH_MAX_HOPS; h++)
		profile_hops(&chain, h, &st[h - 1], &e2e[h - 1], base);

	printf("===== end-to-end (net cycles) =====\n");
	printf("%-4s %10s %10s %12s\n", "hop", "e2e_net", "delta", "dfs_visits");
	for (h = 1; h <= BENCH_MAX_HOPS; h++) {
		uint64_t d = (h == 1) ? e2e[0] : (e2e[h - 1] - e2e[h - 2]);
		uint64_t dv = st[h - 1].dfs_visits / PROF_REPS;

		printf("%-4u %10" PRIu64 " %10" PRIu64 " %12" PRIu64 "\n",
		       h, e2e[h - 1], d, dv);
	}

	printf("\n===== per-phase avg (/%d, nested RDTSC tax) =====\n",
	       PROF_REPS);
	for (h = 1; h <= BENCH_MAX_HOPS; h++) {
		uint64_t n = PROF_REPS;
		uint64_t gb = st[h - 1].gate_build / n;
		uint64_t rt = st[h - 1].route / n;
		uint64_t ie = st[h - 1].iso_enter / n;
		uint64_t ab = st[h - 1].arbiter / n;
		uint64_t mp = st[h - 1].map_lookup / n;
		uint64_t ca = st[h - 1].callee / n;
		uint64_t il = st[h - 1].iso_leave / n;
		uint64_t sum = gb + rt + ie + ab + mp + ca + il;
		uint64_t dv = st[h - 1].dfs_visits / n;

		printf("\n--- hop=%u e2e=%" PRIu64 " dfs_visits=%" PRIu64 " ---\n",
		       h, e2e[h - 1], dv);
		print_phase_row("gate_build", gb, sum);
		print_phase_row("route(DFS)", rt, sum);
		print_phase_row("iso_enter", ie, sum);
		print_phase_row("arbiter", ab, sum);
		print_phase_row("map_lookup", mp, sum);
		print_phase_row("callee", ca, sum);
		print_phase_row("iso_leave", il, sum);
		printf("  %-14s %8" PRIu64 "\n", "sum(phases)", sum);
	}

	/* Ablation: how much of the hop growth is iso / map / arbiter */
	{
		static const struct {
			const char *name;
			uint32_t flags;
		} modes[] = {
			{ "full", FABRIC_ABL_NONE },
			{ "skip_iso", FABRIC_ABL_SKIP_ISO },
			{ "skip_arb", FABRIC_ABL_SKIP_ARBITER },
			{ "no_map", FABRIC_ABL_DIRECT_FN },
			{ "bare", FABRIC_ABL_SKIP_ISO | FABRIC_ABL_SKIP_ARBITER |
				  FABRIC_ABL_DIRECT_FN },
		};
		unsigned m, i;
		uint64_t te[BENCH_MAX_HOPS];

		printf("\n===== e2e by ablation (net) =====\n");
		printf("%-10s", "mode");
		for (h = 1; h <= BENCH_MAX_HOPS; h++)
			printf(" %6u", h);
		printf("\n");

		for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
			fabric_ablation_set(modes[m].flags);
			for (h = 1; h <= BENCH_MAX_HOPS; h++) {
				for (i = 0; i < WARMUP; i++)
					call_hops(&chain, h);
				te[h - 1] = measure_e2e_min(&chain, h, base);
			}
			printf("%-10s", modes[m].name);
			for (h = 1; h <= BENCH_MAX_HOPS; h++)
				printf(" %6" PRIu64, te[h - 1]);
			printf("\n");
		}
		fabric_ablation_set(FABRIC_ABL_NONE);
	}

	scale_chain_destroy(&chain);
	printf("\n===== layers bench done =====\n");
	return 0;
}
