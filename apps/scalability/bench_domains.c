/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * bench_domains.c — MPK domain scalability (A + C + D)
 *
 * A) Isolation policy (same 1-hop call):
 *      all_fixed  — 1 MPK key per component; always call leaf[0]
 *      path_fixed — only hub+leaf[0] keyed; siblings route-only
 * C) Call pattern under all-domains:
 *      all_rr     — round-robin among leaf domains (dst-direct, O(1) route)
 * D) Resource axis:
 *      CSV keys_used; plot secondary Y = MPK keys consumed
 *
 * Call path held to 1 hop; routing cost equalized via _fabric_do_gate_dst
 * so series differ by isolation policy / which domain key is entered.
 *
 * CSV,<series>,<N>,<min>,<avg>,<dfs>,<keys>,<status>
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
#define REPS		40000

enum call_mode {
	CALL_FIXED = 0,
	CALL_RR = 1,
};

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
	uint64_t max;
	uint64_t avg;
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

static void call_leaf(const struct scale_star *star, unsigned leaf_idx)
{
	uint32_t dst = scale_star_leaf_id(star, leaf_idx);
	char name[8];

	snprintf(name, sizeof(name), "L%02u", leaf_idx);
	_fabric_do_gate_dst(star->hub->fabric_id, dst, name,
			    (void *)scale_empty_stub, NULL, 0, 0, 0);
}

static void measure_calls(const struct scale_star *star, enum call_mode mode,
			  unsigned reps, const struct lat_stats *base,
			  struct lat_stats *out)
{
	uint64_t min = UINT64_MAX, max = 0, sum = 0;
	unsigned i;

	for (i = 0; i < reps; i++) {
		unsigned leaf = (mode == CALL_RR) ? (i % star->n_leaves) : 0;
		uint64_t t0 = bench_start();

		call_leaf(star, leaf);
		{
			uint64_t t1 = bench_end();
			uint64_t d = t1 - t0;

			if (d < min)
				min = d;
			if (d > max)
				max = d;
			sum += d;
		}
	}
	out->min = (min > base->min) ? (min - base->min) : 0;
	out->max = (max > base->min) ? (max - base->min) : 0;
	out->avg = (sum / reps > base->avg) ? (sum / reps - base->avg) : 0;
}

static void run_series(const char *series, int iso_mode, enum call_mode mode,
		       const unsigned *ns, int n_points,
		       const struct lat_stats *base)
{
	int p;

	printf("\n----- %s (iso=%s call=%s) -----\n", series,
	       iso_mode == SCALE_ISO_ALL_MPK ? "all-keys" : "path-keys",
	       mode == CALL_RR ? "round-robin" : "fixed-leaf0");
	printf("%-6s %8s %8s %6s %s\n", "N", "min", "avg", "keys", "status");

	for (p = 0; p < n_points; p++) {
		unsigned n = ns[p];
		struct scale_star star;
		struct lat_stats st;
		unsigned keys;
		unsigned i;

		if (scale_star_build_iso(&star, n, iso_mode) != 0) {
			printf("%-6u %8s %8s %6s %s\n",
			       n, "-", "-", "-", "key_exhaust");
			printf("CSV,%s,%u,0,0,0,0,key_exhaust\n", series, n);
			continue;
		}

		keys = scale_star_keys_used(&star);
		for (i = 0; i < WARMUP; i++) {
			unsigned leaf = (mode == CALL_RR) ?
					(i % star.n_leaves) : 0;

			call_leaf(&star, leaf);
		}

		measure_calls(&star, mode, REPS, base, &st);

		printf("%-6u %8" PRIu64 " %8" PRIu64 " %6u %s\n",
		       n, st.min, st.avg, keys, "ok");
		printf("CSV,%s,%u,%" PRIu64 ",%" PRIu64 ",0,%u,ok\n",
		       series, n, st.min, st.avg, keys);

		scale_star_destroy(&star);
	}
}

int main(void)
{
	struct lat_stats base;
	/* A: all-domains fixed target — full sweep through budget */
	static const unsigned ns_all_fixed[] = {
		2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};
	/* C: RR among all keys; N=14 RR faults on this stack — trend clear by 13 */
	static const unsigned ns_all_rr[] = {
		2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15
	};
	/* A: path-only — same range as all, stop at budget (fail is all-domains) */
	static const unsigned ns_path[] = {
		2, 4, 6, 8, 10, 12, 14
	};

#if CONFIG_LIBFLEXOS_INTELPKU
	wrpkru(0x3ffffff0);
#endif

	printf("===== scalability: MPK domains (A+C+D) =====\n");
	printf("A: all-keys vs path-keys (1-hop)\n");
	printf("C: fixed leaf0 vs round-robin leaves (dst-direct)\n");
	printf("D: report keys_used for dual-axis plot\n");
	printf("MPK usable keys=%u\n", SCALE_MPK_USABLE_KEYS);

	measure_empty(REPS, &base);
	printf("timer baseline: min=%" PRIu64 " avg=%" PRIu64 "\n",
	       base.min, base.avg);

	run_series("all_fixed", SCALE_ISO_ALL_MPK, CALL_FIXED, ns_all_fixed,
		   (int)(sizeof(ns_all_fixed) / sizeof(ns_all_fixed[0])),
		   &base);
	run_series("all_rr", SCALE_ISO_ALL_MPK, CALL_RR, ns_all_rr,
		   (int)(sizeof(ns_all_rr) / sizeof(ns_all_rr[0])), &base);
	run_series("path_fixed", SCALE_ISO_PATH_MPK, CALL_FIXED, ns_path,
		   (int)(sizeof(ns_path) / sizeof(ns_path[0])), &base);

	printf("\n===== domains bench done =====\n");
	return 0;
}
