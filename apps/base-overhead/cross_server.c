/* SPDX-License-Identifier: BSD-3-Clause */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example1/isolated.h>
#include <example2/isolated.h>
#include <example3/isolated.h>

#define REPS 1000000

// benchmark 入口
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

// benchmark出口
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

// 空函数调用，测量最小的函数调用开销
__attribute__((noinline))
static void empty_fcall(void)
{
	asm volatile("");
}

// 测试计时器的最小开销，作为基线
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


static uint64_t measure_akira_empty_fcall(uint64_t empty_overhead)
{	
	uint64_t min_cycles = UINT64_MAX;
	uint64_t t0;
	uint64_t t1;
	int i;

	for (i = 0; i < REPS; i++) {
        t0 = bench_start();
        fabric_gate(0, example2_empty);
        t1 = bench_end();
        if ((t1 - t0) < min_cycles)
            min_cycles = t1 - t0;
    }

    return min_cycles - empty_overhead; 
	// return min_cycles;
}

int main(void)
{   
	flexos_nop_gate(0, 0, printf,
			"Starting base overhead benchmarks...\n");
	// 计时成本开销
	uint64_t empty_overhead;
	empty_overhead = measure_empty_overhead();

	flexos_nop_gate(0, 0, printf,
			"empty_overhead (baseline): %" PRIu64 " cycles\n",
			empty_overhead);

	// 测量同隔间fabric_gate的开销
	uint64_t same_comp_fabric_gate_empty_fcall;
	same_comp_fabric_gate_empty_fcall = measure_akira_empty_fcall(empty_overhead);

	
	flexos_nop_gate(0, 0, printf,
			"same_comp_fabric_gate_empty_fcall: %" PRIu64 " cycles\n",
			same_comp_fabric_gate_empty_fcall);

	return 0;
}