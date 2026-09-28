/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * cross_domain_ablation.c — Part B style: per-phase breakdown by hop count
 *
 * For each N-hop chain, accumulate phase cycles + DFS node visits.
 * Report avg per chain and share %. Compare N vs N-1 phase deltas
 * to see why marginal cost grows past ~1x T(1).
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example2/isolated.h>
#include <overhead.h>
#include <backends.h>
#include "../../../unikraft/lib/fabric-core/message/include/message.h"
#include "fabric_init.h"

#define WARMUP		10000
#define PROF_REPS	100000
#define E2E_REPS	500000
#define PARTC_WARMUP	10000
#define PARTC_REPS	50000
#define PARTC_BATCH	64
#define PARTD_BATCH	64
#define PARTD_REPS	50000
#define PARTD_WARMUP	10000

static int partc_u64_cmp(const void *a, const void *b)
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

static uint64_t measure_batch_min(void (*fn)(void))
{
	uint64_t min = UINT64_MAX;
	int i, j;

	for (i = 0; i < PARTD_REPS; i++) {
		uint64_t t0, t1, dt;

		t0 = bench_start();
		for (j = 0; j < PARTD_BATCH; j++)
			fn();
		t1 = bench_end();
		dt = (t1 - t0) / PARTD_BATCH;
		if (dt < min)
			min = dt;
	}
	return min;
}

__attribute__((noinline))
static void chain_1(void)
{
	asm volatile("");
	fabric_gate(0, example2_empty);
}

__attribute__((noinline))
static void chain_2(void)
{
	asm volatile("");
	fabric_gate(0, e2c1);
}

__attribute__((noinline))
static void chain_3(void)
{
	asm volatile("");
	fabric_gate(0, e2c2);
}

__attribute__((noinline))
static void chain_4(void)
{
	asm volatile("");
	fabric_gate(0, e2c3);
}

/* Same-domain / Unikraft-style direct call (no fabric message path). */
__attribute__((noinline))
static void direct_call_empty(void)
{
	asm volatile("");
	example2_empty();
}

__attribute__((noinline))
static void fabric_call_empty(void)
{
	asm volatile("");
	fabric_gate(0, example2_empty);
}

/*
 * Three-lib chain ablation (lib1=app, lib2=example2, lib3=example3):
 *   case1_all_direct  — app→e2→e3 all direct calls
 *   case2_akira_hybrid — app→e2 via fabric, e2→e3 direct
 *   case3_all_message  — app→e2→e3 all fabric_gate (microkernel-like)
 */
__attribute__((noinline))
static void case1_all_direct(void)
{
	asm volatile("");
	e2_call_e3_direct();
}

__attribute__((noinline))
static void case2_akira_hybrid(void)
{
	asm volatile("");
	fabric_gate(0, e2_call_e3_direct);
}

__attribute__((noinline))
static void case3_all_message(void)
{
	asm volatile("");
	fabric_gate(0, e2_call_e3_msg);
}

static void (*const chains[4])(void) = {
	chain_1, chain_2, chain_3, chain_4
};

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

static uint64_t measure_e2e_min(void (*fn)(void), uint64_t base)
{
	uint64_t min = UINT64_MAX;
	int i;

	for (i = 0; i < E2E_REPS; i++) {
		uint64_t t0 = bench_start();
		fn();
		uint64_t t1 = bench_end();
		if ((t1 - t0) < min)
			min = t1 - t0;
	}
	return min - base;
}

static void print_phase_row(const char *name, uint64_t v, uint64_t sum)
{
	uint64_t pct = sum ? (v * 100) / sum : 0;

	flexos_nop_gate(0, 0, printf,
			"  %-14s %8" PRIu64 "  %3" PRIu64 "%%\n",
			name, v, pct);
}

static void profile_chain(int hops, void (*fn)(void),
			  struct fabric_overhead_stats *out,
			  uint64_t *e2e_out, uint64_t base)
{
	int i;

	fabric_ablation_set(FABRIC_ABL_NONE);

	for (i = 0; i < WARMUP; i++)
		fn();

	*e2e_out = measure_e2e_min(fn, base);

	fabric_overhead_reset();
	fabric_overhead_enable(1);
	for (i = 0; i < PROF_REPS; i++)
		fn();
	fabric_overhead_enable(0);
	fabric_overhead_get(out);
}

int main(void)
{
	uint64_t base;
	struct fabric_overhead_stats st[4];
	uint64_t e2e[4];
	uint64_t sum[4];
	int h;

	flexos_nop_gate(0, 0, printf,
			"===== cross_domain Part B (per-phase by hop) =====\n");
	flexos_nop_gate(0, 0, printf,
			"Phase cycles are inflated by nested RDTSC; use %% and\n");
	flexos_nop_gate(0, 0, printf,
			"dfs_visits (exact) to see what grows with hop count.\n\n");

	base = measure_empty();
	flexos_nop_gate(0, 0, printf,
			"timer baseline: %" PRIu64 "\n\n", base);

	for (h = 0; h < 4; h++)
		profile_chain(h + 1, chains[h], &st[h], &e2e[h], base);

	flexos_nop_gate(0, 0, printf,
			"===== end-to-end (Part A style, for reference) =====\n");
	flexos_nop_gate(0, 0, printf,
			"hop  e2e_net   delta(n-(n-1))\n");
	for (h = 0; h < 4; h++) {
		uint64_t d = (h == 0) ? e2e[0] : (e2e[h] - e2e[h - 1]);

		flexos_nop_gate(0, 0, printf,
				"%d    %8" PRIu64 "   %8" PRIu64 "\n",
				h + 1, e2e[h], d);
	}

	flexos_nop_gate(0, 0, printf,
			"\n===== Part B: avg cycles per chain invocation =====\n");
	flexos_nop_gate(0, 0, printf,
			"(totals / %d reps; nested RDTSC tax included)\n",
			PROF_REPS);

	for (h = 0; h < 4; h++) {
		uint64_t n = PROF_REPS;
		uint64_t gb = st[h].gate_build / n;
		uint64_t rt = st[h].route / n;
		uint64_t ie = st[h].iso_enter / n;
		uint64_t ab = st[h].arbiter / n;
		uint64_t mp = st[h].map_lookup / n;
		uint64_t ca = st[h].callee / n;
		uint64_t il = st[h].iso_leave / n;
		uint64_t dv = st[h].dfs_visits / n;

		sum[h] = gb + rt + ie + ab + mp + ca + il;

		flexos_nop_gate(0, 0, printf,
				"\n--- %d-hop (e2e=%" PRIu64 ", fabric_gates=%" PRIu64
				", dfs_visits/chain=%" PRIu64 ") ---\n",
				h + 1, e2e[h], st[h].samples / n, dv);
		print_phase_row("gate_build", gb, sum[h]);
		print_phase_row("route(DFS)", rt, sum[h]);
		print_phase_row("iso_enter", ie, sum[h]);
		print_phase_row("arbiter", ab, sum[h]);
		print_phase_row("map_lookup", mp, sum[h]);
		print_phase_row("callee", ca, sum[h]);
		print_phase_row("iso_leave", il, sum[h]);
		flexos_nop_gate(0, 0, printf,
				"  %-14s %8" PRIu64 "\n", "sum(phases)", sum[h]);
	}

	flexos_nop_gate(0, 0, printf,
			"\n===== Marginal phase delta: hop(n) - hop(n-1) =====\n");
	flexos_nop_gate(0, 0, printf,
			"         %10s %10s %10s\n",
			"2-1", "3-2", "4-3");
	{
		uint64_t n = PROF_REPS;
		const char *names[] = {
			"gate_build", "route(DFS)", "iso_enter", "arbiter",
			"map_lookup", "callee", "iso_leave", "dfs_visits"
		};
		uint64_t v[4][8];
		int p;

		for (h = 0; h < 4; h++) {
			v[h][0] = st[h].gate_build / n;
			v[h][1] = st[h].route / n;
			v[h][2] = st[h].iso_enter / n;
			v[h][3] = st[h].arbiter / n;
			v[h][4] = st[h].map_lookup / n;
			v[h][5] = st[h].callee / n;
			v[h][6] = st[h].iso_leave / n;
			v[h][7] = st[h].dfs_visits / n;
		}

		for (p = 0; p < 8; p++) {
			flexos_nop_gate(0, 0, printf,
					"%-10s %10" PRIu64 " %10" PRIu64 " %10" PRIu64 "\n",
					names[p],
					v[1][p] - v[0][p],
					v[2][p] - v[1][p],
					v[3][p] - v[2][p]);
		}
	}

	flexos_nop_gate(0, 0, printf,
			"\nIf dfs_visits marginal grows (e.g. 2,3,4,...), bidi DFS\n");
	flexos_nop_gate(0, 0, printf,
			"is exploring upstream before the next hop — that explains\n");
	flexos_nop_gate(0, 0, printf,
			"why delta is >~T(1) and rises with depth.\n");

	/*
	 * Part A: same e2e under ablation — which knob shrinks delta?
	 *
	 * Iso split (same backend):
	 *   C_total = full − skip_iso          (whole transition path)
	 *   C_mech  = iso_mech − skip_iso      (mechanism switch only)
	 *   C_sw    = full − iso_mech          (bridge software path)
	 * with C_mech + C_sw == C_total.
	 */
	{
		static const struct {
			const char *name;
			uint32_t flags;
		} modes[] = {
			{ "full", FABRIC_ABL_NONE },
			{ "iso_mech", FABRIC_ABL_ISO_MECH_ONLY },
			{ "skip_iso", FABRIC_ABL_SKIP_ISO },
			{ "skip_arb", FABRIC_ABL_SKIP_ARBITER },
			{ "no_map", FABRIC_ABL_DIRECT_FN },
			{ "no_iso+map", FABRIC_ABL_SKIP_ISO | FABRIC_ABL_DIRECT_FN },
			{ "bare", FABRIC_ABL_SKIP_ISO | FABRIC_ABL_SKIP_ARBITER |
				  FABRIC_ABL_DIRECT_FN },
		};
		int m, i;
		uint64_t te[4];
		uint64_t t_full[4], t_mech[4], t_skip[4];

		flexos_nop_gate(0, 0, printf,
				"\n===== Part A: e2e by ablation (net) =====\n");
		flexos_nop_gate(0, 0, printf,
				"iso_mech = mechanism only (rdpkru/wrpkru); "
				"skip_iso = no enter/leave\n");
		flexos_nop_gate(0, 0, printf,
				"%-12s %8s %8s %8s %8s  %8s %8s %8s\n",
				"mode", "1h", "2h", "3h", "4h",
				"d21", "d32", "d43");

		for (m = 0; m < (int)(sizeof(modes) / sizeof(modes[0])); m++) {
			fabric_ablation_set(modes[m].flags);
			for (h = 0; h < 4; h++) {
				for (i = 0; i < WARMUP; i++)
					chains[h]();
				te[h] = measure_e2e_min(chains[h], base);
			}
			if (modes[m].flags == FABRIC_ABL_NONE) {
				for (h = 0; h < 4; h++)
					t_full[h] = te[h];
			} else if (modes[m].flags == FABRIC_ABL_ISO_MECH_ONLY) {
				for (h = 0; h < 4; h++)
					t_mech[h] = te[h];
			} else if (modes[m].flags == FABRIC_ABL_SKIP_ISO) {
				for (h = 0; h < 4; h++)
					t_skip[h] = te[h];
			}
			flexos_nop_gate(0, 0, printf,
					"%-12s %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " %8" PRIu64
					"  %8" PRIu64 " %8" PRIu64 " %8" PRIu64 "\n",
					modes[m].name,
					te[0], te[1], te[2], te[3],
					te[1] - te[0], te[2] - te[1], te[3] - te[2]);
		}
		fabric_ablation_set(FABRIC_ABL_NONE);

		flexos_nop_gate(0, 0, printf,
				"\n===== Iso peel: C_mech / C_sw / C_total (cycles) =====\n");
		flexos_nop_gate(0, 0, printf,
				"C_mech = iso_mech - skip_iso;  "
				"C_sw = full - iso_mech;  "
				"C_total = full - skip_iso\n");
		flexos_nop_gate(0, 0, printf,
				"note: e2e min often cannot resolve C_sw; "
				"see Part C microbench for C_sw vs C_mech\n");
		flexos_nop_gate(0, 0, printf,
				"%-6s %8s %8s %8s %8s\n",
				"hop", "C_mech", "C_sw", "C_total", "sum_chk");
		for (h = 0; h < 4; h++) {
			int64_t c_mech = (int64_t)t_mech[h] - (int64_t)t_skip[h];
			int64_t c_sw = (int64_t)t_full[h] - (int64_t)t_mech[h];
			int64_t c_tot = (int64_t)t_full[h] - (int64_t)t_skip[h];
			int64_t sum = c_mech + c_sw;

			flexos_nop_gate(0, 0, printf,
					"%d-hop  %8" PRId64 " %8" PRId64 " %8" PRId64 " %8" PRId64 "\n",
					h + 1, c_mech, c_sw, c_tot, sum);
		}
	}

	/*
	 * Part C: enter/leave path microbench (min + median).
	 * Isolates bridge software vs mechanism without full gate/DFS noise.
	 */
	{
		static uint64_t samples[PARTC_REPS];
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
				"\n===== Part C: iso path microbench (min/median) =====\n");
		flexos_nop_gate(0, 0, printf,
				"sw_only = check+lookup (no wrpkru); "
				"mech_only = rdpkru/wrpkru; "
				"full_path = both\n");
		flexos_nop_gate(0, 0, printf,
				"each sample = %d iters in one timed window, "
				"report per-iter cycles (no empty subtract)\n",
				PARTC_BATCH);

		if (!g_comp1_fabric_1 || !g_comp1_fabric_1->bridge ||
		    !g_comp1_fabric_2) {
			flexos_nop_gate(0, 0, printf,
					"Part C skipped: fabrics not ready\n");
			return 0;
		}

		br = g_comp1_fabric_1->bridge;
		memset(&msg, 0, sizeof(msg));
		msg.src_fabric_id = g_comp1_fabric_1->fabric_id;
		msg.dst_fabric_id = g_comp1_fabric_2->fabric_id;

		for (k = 0; k < 3; k++) {
			uint64_t minv = UINT64_MAX;

			for (i = 0; i < PARTC_WARMUP; i++) {
				rc = kernels[k].fn(br, &msg);
				if (rc != 0) {
					flexos_nop_gate(0, 0, printf,
							"%s failed rc=%d\n",
							kernels[k].name, rc);
					return 0;
				}
			}

			for (i = 0; i < PARTC_REPS; i++) {
				uint64_t t0, t1, dt;

				t0 = bench_start();
				for (j = 0; j < PARTC_BATCH; j++)
					kernels[k].fn(br, &msg);
				t1 = bench_end();
				dt = (t1 - t0) / PARTC_BATCH;

				samples[i] = dt;
				if (dt < minv)
					minv = dt;
			}

			qsort(samples, PARTC_REPS, sizeof(samples[0]),
			      partc_u64_cmp);
			vmin[k] = minv;
			vmed[k] = samples[PARTC_REPS / 2];

			flexos_nop_gate(0, 0, printf,
					"%-10s  min=%8" PRIu64 "  median=%8" PRIu64 "\n",
					kernels[k].name, vmin[k], vmed[k]);
		}

		flexos_nop_gate(0, 0, printf,
				"\n----- Part C peel (median; cycles) -----\n");
		flexos_nop_gate(0, 0, printf,
				"C_sw=sw_only  C_mech=mech_only  "
				"C_total=full_path  sum=C_sw+C_mech\n");
		flexos_nop_gate(0, 0, printf,
				"C_sw=%" PRIu64 "  C_mech=%" PRIu64
				"  C_total=%" PRIu64 "  sum=%" PRIu64
				"  (share of sum: sw=%" PRIu64 "%% mech=%" PRIu64 "%%)\n",
				vmed[0], vmed[1], vmed[2],
				vmed[0] + vmed[1],
				(vmed[0] + vmed[1])
					? (vmed[0] * 100) / (vmed[0] + vmed[1]) : 0,
				(vmed[0] + vmed[1])
					? (vmed[1] * 100) / (vmed[0] + vmed[1]) : 0);
		flexos_nop_gate(0, 0, printf,
				"(min) C_sw=%" PRIu64 "  C_mech=%" PRIu64
				"  C_total=%" PRIu64 "  sum=%" PRIu64 "\n",
				vmin[0], vmin[1], vmin[2],
				vmin[0] + vmin[1]);
		flexos_nop_gate(0, 0, printf,
				"note: sw/mech are separate kernels; sum may "
				"slightly exceed fused full_path\n");
	}

	/*
	 * Part D: three-lib chain — direct vs Akira hybrid vs all-message.
	 *
	 *   case1_all_direct   — lib1→lib2→lib3 all empty direct calls
	 *   case2_akira_hybrid — lib1→lib2 fabric, lib2→lib3 direct
	 *   case3_all_message  — lib1→lib2→lib3 all fabric messages
	 *
	 * Empty callees; batch×64 amortizes RDTSC (no timer-baseline subtract)
	 * so direct-call overhead is visible. Fabric modes use SKIP_ISO.
	 */
	{
		uint64_t t1, t2, t3;
		int i;

		flexos_nop_gate(0, 0, printf,
				"\n===== Part D: direct vs hybrid vs all-message =====\n");
		flexos_nop_gate(0, 0, printf,
				"lib1=app  lib2=example2  lib3=example3\n");
		flexos_nop_gate(0, 0, printf,
				"leaf=example3_empty; batch=%d, no timer subtract\n",
				PARTD_BATCH);
		flexos_nop_gate(0, 0, printf,
				"case1: all direct calls\n");
		flexos_nop_gate(0, 0, printf,
				"case2: fabric(lib1→lib2) + direct(lib2→lib3)  [Akira]\n");
		flexos_nop_gate(0, 0, printf,
				"case3: fabric(lib1→lib2) + fabric(lib2→lib3)  [all message]\n");

		for (i = 0; i < PARTD_WARMUP; i++)
			case1_all_direct();
		t1 = measure_batch_min(case1_all_direct);

		fabric_ablation_set(FABRIC_ABL_SKIP_ISO);
		for (i = 0; i < PARTD_WARMUP; i++)
			case2_akira_hybrid();
		t2 = measure_batch_min(case2_akira_hybrid);

		for (i = 0; i < PARTD_WARMUP; i++)
			case3_all_message();
		t3 = measure_batch_min(case3_all_message);
		fabric_ablation_set(FABRIC_ABL_NONE);

		flexos_nop_gate(0, 0, printf,
				"%-18s %8" PRIu64 " cycles\n",
				"case1_all_direct", t1);
		flexos_nop_gate(0, 0, printf,
				"%-18s %8" PRIu64 " cycles\n",
				"case2_akira", t2);
		flexos_nop_gate(0, 0, printf,
				"%-18s %8" PRIu64 " cycles\n",
				"case3_all_message", t3);
		flexos_nop_gate(0, 0, printf,
				"\nAkira extra vs direct   = %" PRId64 "\n",
				(int64_t)t2 - (int64_t)t1);
		flexos_nop_gate(0, 0, printf,
				"All-msg extra vs Akira  = %" PRId64 "\n",
				(int64_t)t3 - (int64_t)t2);
		flexos_nop_gate(0, 0, printf,
				"All-msg extra vs direct = %" PRId64 "\n",
				(int64_t)t3 - (int64_t)t1);
	}

	return 0;
}
