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

/* ---------- 与 FlexOS microbenchmark 一致的计时器 ---------- */

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

/* ---------- 论文里的 "function" baseline：直接空调用 ---------- */

__attribute__((noinline))
static void empty_fcall(void)
{
	asm volatile("");
}

/* 对应 FlexOS 的 overhead_tsc：纯计时框架开销 */
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

/*
 * 对应 FlexOS 的:
 *   overhead_fcall = min(bench_start(); RUN_FCALL(); bench_end())
 *   fcall = overhead_fcall - overhead_tsc
 *
 * 论文图 11b 的 "function = 2" 就是这里的返回值。
 */
static uint64_t measure_plain_fcall_net(uint64_t *raw_fcall_out)
{
	uint64_t empty_overhead;
	uint64_t raw_fcall;
	int ok = 0;

	while (!ok) {
		empty_overhead = measure_empty_overhead();

		raw_fcall = UINT64_MAX;
		for (int i = 0; i < REPS; i++) {
			uint64_t t0 = bench_start();
			empty_fcall();   /* 直接 call，不要 fabric_gate */
			uint64_t t1 = bench_end();

			if ((t1 - t0) < raw_fcall)
				raw_fcall = t1 - t0;
		}

		/* FlexOS 原版也有这个校验 */
		if (raw_fcall > empty_overhead)
			ok = 1;
	}

	if (raw_fcall_out)
		*raw_fcall_out = raw_fcall;

	return raw_fcall - empty_overhead;
}

/* ---------- 你自己的架构测量，保留但不参与论文对比 ---------- */

static uint64_t measure_fabric_gate_raw(uint64_t empty_overhead)
{
	uint64_t min_cycles = UINT64_MAX;
	int i;

	for (i = 0; i < REPS; i++) {
		uint64_t t0 = bench_start();
		fabric_gate(0, example2_empty);
		uint64_t t1 = bench_end();

		if ((t1 - t0) < min_cycles)
			min_cycles = t1 - t0;
	}

	return min_cycles - empty_overhead;
}

int main(void)
{
	uint64_t empty_overhead;
	uint64_t raw_fcall;
	uint64_t plain_fcall_net;
	uint64_t fabric_gate_net;

	flexos_nop_gate(0, 0, printf,
			"Starting FlexOS-style function-call benchmark...\n");

	/* 论文 "function=2" 的完整测量 */
	plain_fcall_net = measure_plain_fcall_net(&raw_fcall);
	empty_overhead = measure_empty_overhead();

	flexos_nop_gate(0, 0, printf,
			"\n#instruction,latency\n");
	flexos_nop_gate(0, 0, printf,
			"fcall,%" PRIu64 "\n", plain_fcall_net);

	flexos_nop_gate(0, 0, printf,
			"\n--- debug breakdown ---\n");
	flexos_nop_gate(0, 0, printf,
			"empty_overhead (timing only): %" PRIu64 " cycles\n",
			empty_overhead);
	flexos_nop_gate(0, 0, printf,
			"raw_fcall (timing + call):    %" PRIu64 " cycles\n",
			raw_fcall);
	flexos_nop_gate(0, 0, printf,
			"plain_fcall net (paper value): %" PRIu64 " cycles\n",
			plain_fcall_net);

	/* 你的架构测量，单独看 */
	fabric_gate_net = measure_fabric_gate_raw(empty_overhead);
	flexos_nop_gate(0, 0, printf,
			"fabric_gate net (your arch):  %" PRIu64 " cycles\n",
			fabric_gate_net);

	return 0;
}