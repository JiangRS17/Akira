/* SPDX-License-Identifier: BSD-3-Clause */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example2/isolated.h>

#define REPS 1000000
/* ---------------- cycle counter ---------------- */

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

/* ---------------- baseline ---------------- */

static uint64_t measure_empty_overhead(void)
{
	uint64_t min_cycles = UINT64_MAX;
	uint64_t t0;
	uint64_t t1;
	int i;

	for (i = 0; i < REPS; i++) {
		t0 = bench_start();
		asm volatile("");
		t1 = bench_end();
		if ((t1 - t0) < min_cycles)
			min_cycles = t1 - t0;
	}

	return min_cycles;
}

/* ---------------- chain functions (from fabric-1) ---------------- */

/* 1 cross-domain: fabric-1 -> fabric-2 */
__attribute__((noinline))
static void one_cross_domain_chain(void)
{
	asm volatile("");
	fabric_gate(0, example2_empty);
}

/* 2 cross-domains: fabric-1 -> fabric-2 -> fabric-3 */
__attribute__((noinline))
static void two_cross_domain_chain(void)
{
	asm volatile("");
	fabric_gate(0, e2c1); 
}

/* 3 cross-domains: fabric-1 -> fabric-2 -> fabric-3 -> fabric-4 */
__attribute__((noinline))
static void three_cross_domain_chain(void)
{
	asm volatile("");
	fabric_gate(0, e2c2);
}

/* 4 cross-domains: fabric-1 -> fabric-2 -> fabric-3 -> fabric-4 -> fabric-5 */
__attribute__((noinline))
static void four_cross_domain_chain(void)
{
	asm volatile("");
	fabric_gate(0, e2c3);
}

static uint64_t measure_empty_chain(void (*fn)(void), uint64_t empty_overhead)
{
	uint64_t min_cycles = UINT64_MAX;
	uint64_t t0;
	uint64_t t1;
	int i;

	for (i = 0; i < REPS; i++) {
		t0 = bench_start();
		fn();
		t1 = bench_end();
		if ((t1 - t0) < min_cycles)
			min_cycles = t1 - t0;
	}

	return min_cycles - empty_overhead;
}

/* ---------------- main ---------------- */

int main(void)
{
	uint64_t empty_overhead;
	uint64_t one_cross;
	uint64_t two_cross;
	uint64_t three_cross;
	uint64_t four_cross;

	flexos_nop_gate(0, 0, printf,
			"Starting cross-domain benchmarks...\n");

	empty_overhead = measure_empty_overhead();
	flexos_nop_gate(0, 0, printf,
			"empty_overhead (baseline): %" PRIu64 " cycles\n",
			empty_overhead);

	one_cross = measure_empty_chain(one_cross_domain_chain, empty_overhead);
	two_cross = measure_empty_chain(two_cross_domain_chain, empty_overhead);
	three_cross = measure_empty_chain(three_cross_domain_chain,
					  empty_overhead);
	four_cross = measure_empty_chain(four_cross_domain_chain,
					 empty_overhead);

	flexos_nop_gate(0, 0, printf,
			"1 cross-domain (fabric-1 -> fabric-2): %" PRIu64 " cycles\n",
			one_cross);
	flexos_nop_gate(0, 0, printf,
			"2 cross-domains (fabric-1 -> fabric-2 -> fabric-3): %" PRIu64 " cycles\n",
			two_cross);
	flexos_nop_gate(0, 0, printf,
			"3 cross-domains (fabric-1 -> fabric-2 -> fabric-3 -> fabric-4): %" PRIu64 " cycles\n",
			three_cross);
	flexos_nop_gate(0, 0, printf,
			"4 cross-domains (fabric-1 -> fabric-2 -> fabric-3 -> fabric-4 -> fabric-5): %" PRIu64 " cycles\n",
			four_cross);

	return 0;
}
