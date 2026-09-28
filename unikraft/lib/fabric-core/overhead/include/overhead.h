/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Fabric/Akira hot-path overhead profile + runtime ablation knobs.
 *
 * Phases (1 cross-fabric hop):
 *   gate_build  — FabricMsg fill + FNV service_id hash
 *   route       — controller DFS
 *   iso_enter   — isolation backend enter (MPK wrpkru …)
 *   arbiter     — wait + release
 *   map_lookup  — function_map_get_by_service_id
 *   callee      — actual function body
 *   iso_leave   — isolation backend leave
 */
#ifndef FABRIC_OVERHEAD_H
#define FABRIC_OVERHEAD_H

#include <stdint.h>

/* Ablation flags (OR together) */
#define FABRIC_ABL_NONE		0u
#define FABRIC_ABL_SKIP_ISO	(1u << 0)  /* skip enter/leave entirely */
#define FABRIC_ABL_SKIP_ARBITER	(1u << 1)  /* skip arbiter wait/release */
#define FABRIC_ABL_DIRECT_FN	(1u << 2)  /* skip map; call msg->func_ptr */
/* enter/leave: mechanism switch only (no check/lookup software path) */
#define FABRIC_ABL_ISO_MECH_ONLY	(1u << 3)

struct fabric_overhead_stats {
	uint64_t gate_build;
	uint64_t route;
	uint64_t iso_enter;
	uint64_t arbiter;
	uint64_t map_lookup;
	uint64_t callee;
	uint64_t iso_leave;
	uint64_t total_ctrl; /* route..iso_leave inside controller */
	uint64_t samples;
	uint64_t dfs_visits; /* nodes touched by execute_dfs (no timer bias) */
};

void fabric_overhead_reset(void);
void fabric_overhead_enable(int on);
int  fabric_overhead_enabled(void);
void fabric_overhead_get(struct fabric_overhead_stats *out);
struct fabric_overhead_stats *fabric_overhead_stats_ptr(void);
void fabric_dfs_visit_add(uint64_t n);

void fabric_ablation_set(uint32_t flags);
uint32_t fabric_ablation_get(void);

/* LFENCE+RDTSC helpers used on the profiled hot path */
static inline uint64_t fabric_ov_tsc(void)
{
	unsigned lo, hi;

	asm volatile("lfence\n\trdtsc\n\t"
		     "mov %%edx, %0\n\tmov %%eax, %1\n\t"
		     : "=r"(hi), "=r"(lo)
		     :
		     : "%rax", "%rdx");
	return ((uint64_t)hi << 32) | lo;
}

static inline void fabric_overhead_add(uint64_t *acc, uint64_t dt)
{
	if (fabric_overhead_enabled() && acc)
		*acc += dt;
}

#endif /* FABRIC_OVERHEAD_H */
