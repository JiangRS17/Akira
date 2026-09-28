/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * iso_path_microbench.c — Peel bridge software vs isolation mechanism
 *
 * Paper claim (ii): additional execution-path cost of the AEU/bridge
 * abstraction on the isolation transition is ≈7% of that transition.
 *
 * Why not e2e skip_iso / iso_mech?
 *   full − skip_iso peels the *whole* enter+leave path (~98 cycles) and
 *   cannot separate mechanism from bridge software. E2E min often also
 *   fails to resolve C_sw (noise). Use these fine-grain kernels instead.
 *
 * Kernels (akira_mpk_path_* in mpk_backend.c):
 *   sw_only   — mpk_check + key lookups, NO wrpkru     → C_sw
 *   mech_only — rdpkru/wrpkru enter+leave only         → C_mech
 *   full_path — complete enter+leave                   → C_total
 *
 * Report: C_sw / (C_sw + C_mech)  (historically ≈ 7 / 101 ≈ 7%).
 *
 * Build: point Makefile.uk APPBASEOVERHEAD_SRCS-y at this file instead of
 *        cross_domain_ablation.c (same helpers), then rebuild + kraft/qemu.
 * Plot:  python3 plot/iso_path_ablation.py
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <overhead.h>
#include <backends.h>
#include "../../../unikraft/lib/fabric-core/message/include/message.h"
#include "fabric_init.h"

#define WARMUP		10000
#define REPS		50000
#define BATCH		64

static int u64_cmp(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a;
	uint64_t y = *(const uint64_t *)b;

	return (x > y) - (x < y);
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

int main(void)
{
	static uint64_t samples[REPS];
	static const struct {
		const char *name;
		int (*fn)(FabricBridge *, FabricMsg *);
	} kernels[] = {
		{ "sw_only", akira_mpk_path_sw_only },
		{ "mech_only", akira_mpk_path_mech_only },
		{ "full_path", akira_mpk_path_full },
	};
	FabricBridge *br;
	FabricMsg msg;
	uint64_t vmin[3], vmed[3];
	int k, i, j, rc;

	fabric_ablation_set(FABRIC_ABL_NONE);

	flexos_nop_gate(0, 0, printf,
			"===== Iso path microbench (bridge SW vs mechanism) =====\n");
	flexos_nop_gate(0, 0, printf,
			"sw_only   = check+lookup (no wrpkru)  → C_sw\n"
			"mech_only = rdpkru/wrpkru             → C_mech\n"
			"full_path = enter+leave fused         → C_total\n"
			"each sample = %d iters / timed window; report per-iter\n\n",
			BATCH);

	/* Fabrics are created by fabric_init_all() (uk_lib_initcall). */
	if (!g_comp1_fabric_1 || !g_comp1_fabric_1->bridge ||
	    !g_comp1_fabric_2) {
		flexos_nop_gate(0, 0, printf, "fabrics not ready\n");
		return 1;
	}

	br = g_comp1_fabric_1->bridge;
	memset(&msg, 0, sizeof(msg));
	msg.src_fabric_id = g_comp1_fabric_1->fabric_id;
	msg.dst_fabric_id = g_comp1_fabric_2->fabric_id;

	for (k = 0; k < 3; k++) {
		uint64_t minv = UINT64_MAX;

		for (i = 0; i < WARMUP; i++) {
			rc = kernels[k].fn(br, &msg);
			if (rc != 0) {
				flexos_nop_gate(0, 0, printf,
						"%s failed rc=%d\n",
						kernels[k].name, rc);
				return 1;
			}
		}

		for (i = 0; i < REPS; i++) {
			uint64_t t0, t1, dt;

			t0 = bench_start();
			for (j = 0; j < BATCH; j++)
				kernels[k].fn(br, &msg);
			t1 = bench_end();
			dt = (t1 - t0) / BATCH;
			samples[i] = dt;
			if (dt < minv)
				minv = dt;
		}

		qsort(samples, REPS, sizeof(samples[0]), u64_cmp);
		vmin[k] = minv;
		vmed[k] = samples[REPS / 2];

		flexos_nop_gate(0, 0, printf,
				"%-10s  min=%8" PRIu64 "  median=%8" PRIu64 "\n",
				kernels[k].name, vmin[k], vmed[k]);
	}

	{
		uint64_t c_sw = vmed[0];
		uint64_t c_mech = vmed[1];
		uint64_t c_tot = vmed[2];
		uint64_t sum = c_sw + c_mech;
		uint64_t sw_pct = sum ? (c_sw * 100) / sum : 0;
		uint64_t mech_pct = sum ? (c_mech * 100) / sum : 0;

		flexos_nop_gate(0, 0, printf,
				"\n----- Peel (median; cycles) -----\n"
				"C_sw=%" PRIu64 "  C_mech=%" PRIu64
				"  C_total=%" PRIu64 "  sum=%" PRIu64 "\n"
				"share of sum: bridge SW=%" PRIu64
				"%%  mechanism=%" PRIu64 "%%\n"
				"(min) C_sw=%" PRIu64 "  C_mech=%" PRIu64
				"  C_total=%" PRIu64 "\n"
				"note: separate kernels; sum may slightly "
				"exceed fused full_path\n"
				"CSV: kernel,min,median\n"
				"sw_only,%" PRIu64 ",%" PRIu64 "\n"
				"mech_only,%" PRIu64 ",%" PRIu64 "\n"
				"full_path,%" PRIu64 ",%" PRIu64 "\n",
				c_sw, c_mech, c_tot, sum, sw_pct, mech_pct,
				vmin[0], vmin[1], vmin[2],
				vmin[0], vmed[0],
				vmin[1], vmed[1],
				vmin[2], vmed[2]);
	}

	return 0;
}
