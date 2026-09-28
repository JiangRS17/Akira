/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * cross_domain_re.c — 对比三种 RDTSC 测量方法
 *
 * 只使用 example2 的链式函数（和 cross_domain.c 一样），
 * 避免直接引用其他 fabric 的函数地址。
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example2/isolated.h>

#define REPS 1000000

/* ---- 方案 A: CPUID+RDTSC（原方案，重量级序列化） ---- */
__attribute__((always_inline)) static inline uint64_t start_a(void)
{
	unsigned lo, hi;
	asm volatile("CPUID\n\tRDTSC\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\t"
		: "=r"(hi), "=r"(lo) : : "%rax","%rbx","%rcx","%rdx");
	return ((uint64_t)hi << 32) | lo;
}
__attribute__((always_inline)) static inline uint64_t end_a(void)
{
	unsigned lo, hi;
	asm volatile("RDTSCP\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\tCPUID\n\t"
		: "=r"(hi), "=r"(lo) : : "%rax","%rbx","%rcx","%rdx");
	return ((uint64_t)hi << 32) | lo;
}

/* ---- 方案 B: 纯 RDTSC（无序列化） ---- */
__attribute__((always_inline)) static inline uint64_t start_b(void)
{
	unsigned lo, hi;
	asm volatile("RDTSC\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\t"
		: "=r"(hi), "=r"(lo) : : "%rax","%rdx");
	return ((uint64_t)hi << 32) | lo;
}
__attribute__((always_inline)) static inline uint64_t end_b(void)
{
	unsigned lo, hi;
	asm volatile("RDTSCP\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\t"
		: "=r"(hi), "=r"(lo) : : "%rax","%rdx");
	return ((uint64_t)hi << 32) | lo;
}

/* ---- 方案 C: LFENCE+RDTSC（轻量序列化） ---- */
__attribute__((always_inline)) static inline uint64_t start_c(void)
{
	unsigned lo, hi;
	asm volatile("lfence\n\tRDTSC\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\t"
		: "=r"(hi), "=r"(lo) : : "%rax","%rdx");
	return ((uint64_t)hi << 32) | lo;
}
__attribute__((always_inline)) static inline uint64_t end_c(void)
{
	unsigned lo, hi;
	asm volatile("RDTSCP\n\t"
		"MOV %%edx, %0\n\tMOV %%eax, %1\n\tlfence\n\t"
		: "=r"(hi), "=r"(lo) : : "%rax","%rdx");
	return ((uint64_t)hi << 32) | lo;
}

/* ---- chain functions ---- */
__attribute__((noinline))
static void chain_1(void) { asm volatile(""); fabric_gate(0, example2_empty); }
__attribute__((noinline))
static void chain_2(void) { asm volatile(""); fabric_gate(0, e2c1); }
__attribute__((noinline))
static void chain_3(void) { asm volatile(""); fabric_gate(0, e2c2); }
__attribute__((noinline))
static void chain_4(void) { asm volatile(""); fabric_gate(0, e2c3); }

/* ---- measure helpers ---- */
static uint64_t measure_pair(uint64_t (*s)(void), uint64_t (*e)(void), void (*fn)(void))
{
	uint64_t min = UINT64_MAX;
	int i;
	for (i = 0; i < REPS; i++) {
		uint64_t t0 = s();
		fn();
		uint64_t t1 = e();
		if ((t1 - t0) < min)
			min = t1 - t0;
	}
	return min;
}

static uint64_t measure_empty(uint64_t (*s)(void), uint64_t (*e)(void))
{
	uint64_t min = UINT64_MAX;
	int i;
	for (i = 0; i < REPS; i++) {
		uint64_t t0 = s();
		asm volatile("");
		uint64_t t1 = e();
		if ((t1 - t0) < min)
			min = t1 - t0;
	}
	return min;
}

/* ---- print helper ---- */
static void run_test(const char *label, uint64_t (*s)(void), uint64_t (*e)(void))
{
	uint64_t base = measure_empty(s, e);
	uint64_t t1, t2, t3, t4;

	t1 = measure_pair(s, e, chain_1);
	t2 = measure_pair(s, e, chain_2);
	t3 = measure_pair(s, e, chain_3);
	t4 = measure_pair(s, e, chain_4);

	flexos_nop_gate(0, 0, printf, "--- %s (base=%" PRIu64 ") ---\n", label, base);
	flexos_nop_gate(0, 0, printf,
		"1-hop: net=%" PRIu64 " per-hop=%" PRIu64 "\n",
		t1 - base, t1 - base);
	flexos_nop_gate(0, 0, printf,
		"2-hop: net=%" PRIu64 " per-hop=%" PRIu64 "\n",
		t2 - base, (t2 - base) / 2);
	flexos_nop_gate(0, 0, printf,
		"3-hop: net=%" PRIu64 " per-hop=%" PRIu64 "\n",
		t3 - base, (t3 - base) / 3);
	flexos_nop_gate(0, 0, printf,
		"4-hop: net=%" PRIu64 " per-hop=%" PRIu64 "\n\n",
		t4 - base, (t4 - base) / 4);
}

int main(void)
{
	flexos_nop_gate(0, 0, printf, "===== RDTSC measurement method comparison =====\n\n");

	run_test("A: CPUID+RDTSC", start_a, end_a);
	run_test("B: pure RDTSC",  start_b, end_b);
	run_test("C: LFENCE+RDTSC", start_c, end_c);

	return 0;
}
