/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * cross_server_ablation.c — 在 cross_server 微基准上做 Akira 开销消融
 *
 * 与 cross_server.c 相同：测 fabric_gate(0, example2_empty)（同隔间 1-hop）
 * 计时器：CPUID+RDTSC（与 cross_server.c 一致）
 *
 * Part A: 端到端消融（逐段关掉）
 * Part B: 分阶段平均 cycles
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example2/isolated.h>
#include <overhead.h>

#define REPS		1000000
#define WARMUP		10000
#define PROF_REPS	200000

__attribute__((always_inline)) static inline uint64_t bench_start(void)
{
	unsigned cycles_low;
	unsigned cycles_high;

	asm volatile(
		"CPUID\n\t"
		"RDTSC\n\t"
		"MOV %%edx, %0\n\t"
		"MOV %%eax, %1\n\t"
		: "=r" (cycles_high), "=r" (cycles_low)
		:
		: "%rax", "%rbx", "%rcx", "%rdx");

	return ((uint64_t) cycles_high << 32) | cycles_low;
}

__attribute__((always_inline)) static inline uint64_t bench_end(void)
{
	unsigned cycles_low;
	unsigned cycles_high;

	asm volatile(
		"RDTSCP\n\t"
		"MOV %%edx, %0\n\t"
		"MOV %%eax, %1\n\t"
		"CPUID\n\t"
		: "=r" (cycles_high), "=r" (cycles_low)
		:
		: "%rax", "%rbx", "%rcx", "%rdx");

	return ((uint64_t) cycles_high << 32) | cycles_low;
}

__attribute__((noinline))
static void empty_fcall(void)
{
	asm volatile("");
}

__attribute__((noinline))
static void akira_empty_fcall(void)
{
	asm volatile("");
	fabric_gate(0, example2_empty);
}

static uint64_t measure_empty_overhead(void)
{
	uint64_t min_cycles = UINT64_MAX;
	int i;

	for (i = 0; i < REPS; i++) {
		uint64_t t0 = bench_start();
		asm volatile("");
		uint64_t t1 = bench_end();
		if ((t1 - t0) < min_cycles)
			min_cycles = t1 - t0;
	}
	return min_cycles;
}

static uint64_t measure_min(void (*fn)(void), int reps)
{
	uint64_t min_cycles = UINT64_MAX;
	int i;

	for (i = 0; i < reps; i++) {
		uint64_t t0 = bench_start();
		fn();
		uint64_t t1 = bench_end();
		if ((t1 - t0) < min_cycles)
			min_cycles = t1 - t0;
	}
	return min_cycles;
}

static void print_ablation(const char *label, uint32_t flags, uint64_t base)
{
	uint64_t net;
	int i;

	fabric_ablation_set(flags);

	for (i = 0; i < WARMUP; i++)
		akira_empty_fcall();

	net = measure_min(akira_empty_fcall, REPS) - base;
	flexos_nop_gate(0, 0, printf,
			"%-44s %8" PRIu64 " cycles\n", label, net);
}

static void print_phase_avg(void)
{
	struct fabric_overhead_stats st;
	int i;
	uint64_t n;

	fabric_ablation_set(FABRIC_ABL_NONE);
	fabric_overhead_reset();
	fabric_overhead_enable(1);

	for (i = 0; i < WARMUP; i++)
		akira_empty_fcall();

	fabric_overhead_reset();
	fabric_overhead_enable(1);
	for (i = 0; i < PROF_REPS; i++)
		akira_empty_fcall();
	fabric_overhead_enable(0);
	fabric_overhead_get(&st);

	n = st.samples ? st.samples : 1;

	flexos_nop_gate(0, 0, printf,
			"\n===== Part B: per-phase avg (full path, n=%" PRIu64 ") =====\n",
			n);
	flexos_nop_gate(0, 0, printf,
			"  gate_build (msg+FNV)     %8" PRIu64 "\n",
			st.gate_build / n);
	flexos_nop_gate(0, 0, printf,
			"  route (DFS)              %8" PRIu64 "\n",
			st.route / n);
	flexos_nop_gate(0, 0, printf,
			"  iso_enter (MPK)          %8" PRIu64 "\n",
			st.iso_enter / n);
	flexos_nop_gate(0, 0, printf,
			"  arbiter wait+release     %8" PRIu64 "\n",
			st.arbiter / n);
	flexos_nop_gate(0, 0, printf,
			"  map_lookup               %8" PRIu64 "\n",
			st.map_lookup / n);
	flexos_nop_gate(0, 0, printf,
			"  callee (empty)           %8" PRIu64 "\n",
			st.callee / n);
	flexos_nop_gate(0, 0, printf,
			"  iso_leave (MPK)          %8" PRIu64 "\n",
			st.iso_leave / n);
	flexos_nop_gate(0, 0, printf,
			"  sum(phases)              %8" PRIu64 "\n",
			(st.gate_build + st.route + st.iso_enter + st.arbiter +
			 st.map_lookup + st.callee + st.iso_leave) / n);
	flexos_nop_gate(0, 0, printf,
			"  controller_total         %8" PRIu64 "\n",
			st.total_ctrl / n);
}

int main(void)
{
	uint64_t base;
	uint64_t plain_fcall;

	flexos_nop_gate(0, 0, printf,
			"===== cross_server ablation (fabric_gate -> example2_empty) =====\n\n");

	base = measure_empty_overhead();
	flexos_nop_gate(0, 0, printf,
			"empty_overhead (baseline): %" PRIu64 " cycles\n", base);

	plain_fcall = measure_min(empty_fcall, REPS) - base;
	flexos_nop_gate(0, 0, printf,
			"plain empty_fcall:         %" PRIu64 " cycles\n\n",
			plain_fcall);

	flexos_nop_gate(0, 0, printf,
			"===== Part A: end-to-end net cycles =====\n");
	print_ablation("full (gate+route+iso+arbiter+map+call)",
		       FABRIC_ABL_NONE, base);
	print_ablation("  - skip iso (enter/leave)",
		       FABRIC_ABL_SKIP_ISO, base);
	print_ablation("  - skip arbiter",
		       FABRIC_ABL_SKIP_ARBITER, base);
	print_ablation("  - direct fn (no map)",
		       FABRIC_ABL_DIRECT_FN, base);
	print_ablation("  - skip iso+arbiter",
		       FABRIC_ABL_SKIP_ISO | FABRIC_ABL_SKIP_ARBITER, base);
	print_ablation("  - skip iso+arbiter+map",
		       FABRIC_ABL_SKIP_ISO | FABRIC_ABL_SKIP_ARBITER |
		       FABRIC_ABL_DIRECT_FN, base);

	print_phase_avg();

	fabric_ablation_set(FABRIC_ABL_NONE);
	return 0;
}
