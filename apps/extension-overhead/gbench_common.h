/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef APPEXTENSIONOVERHEAD_GBENCH_COMMON_H
#define APPEXTENSIONOVERHEAD_GBENCH_COMMON_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <backends.h>
#include <bridge.h>
#include <component.h>
#include <fabric.h>
#include <fabric_error.h>
#include <fabric/isolation.h>
#include <flexos/isolation.h>
#include <uk/config.h>
#if CONFIG_LIBFLEXOS_INTELPKU
#include <flexos/impl/intelpku.h>
#endif
#include <uk/plat/console.h>

#include <example1/isolated.h>
#include <example2/isolated.h>
#include <example3/isolated.h>
#include <example4/isolated.h>
#include <example5/isolated.h>
#include <example6/isolated.h>
#include <example7/isolated.h>
#include <example8/isolated.h>

#define REPS 1000000

struct bench_component {
	const char *name;
	func_t func;
};

struct component_group {
	const struct bench_component *components;
	int count;
};

struct fabric_topology {
	const char *name;
	Fabric *source;
	Fabric *target;
	Fabric *nodes[16];
	int node_count;
};

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

/* Bidirectional soft-bus edge. */
static int fabric_bind_bidi(Fabric *left, Fabric *right)
{
	if (!left || !right || !left->bridge || !right->bridge)
		return FABRIC_ERROR;
	if (fabric_bridge_bind_downstream(left->bridge, right) != FABRIC_SUCCESS)
		return FABRIC_ERROR;
	if (fabric_bridge_bind_downstream(right->bridge, left) != FABRIC_SUCCESS)
		return FABRIC_ERROR;
	return FABRIC_SUCCESS;
}

static Fabric *create_running_fabric(void)
{
	Fabric *fabric = fabric_create();

	if (!fabric || !fabric->component)
		return (void *) 0;

	fabric->component->status = FB_COMP_STATE_RUNNING;
	return fabric;
}

static int register_symbol(Fabric *fabric, const char *name, func_t fn)
{
	return fabric_register_function(fabric, name, fn);
}

/*
 * Enable Fabric MPK isolation: one fabric → one MPK key.
 * Without this, fabric_gate skips iso_enter/iso_leave and the domain-scaling
 * cost underestimates real cross-domain overhead.
 */
static int enable_mpk_isolation(Fabric *fabric)
{
	MPKConfig iso_cfg;

	if (!fabric || !fabric->bridge)
		return FABRIC_ERROR;

	iso_cfg.fabric_id = fabric->fabric_id;
	if (fabric_bridge_set_isolation(fabric->bridge, FABRIC_ISO_MPK,
					&iso_cfg) != FABRIC_SUCCESS)
		return FABRIC_ERROR;
	if (!fabric->bridge->iso_backend || !fabric->bridge->iso_state)
		return FABRIC_ERROR;

	return FABRIC_SUCCESS;
}

static void console_marker(const char *msg)
{
	ukplat_coutk(msg, 0);
}

/*
 * Star: source hub ↔ one fabric per component group.
 * Measured target is the last group so DFS walks more siblings as domains grow.
 * Every fabric (source + domains) gets an MPK isolation backend.
 */
static int build_partitioned_topology(struct fabric_topology *topology,
				      const char *name,
				      const struct component_group groups[],
				      int group_count,
				      int target_group)
{
	Fabric *source;
	Fabric *node;
	int group_index;
	int component_index;
	int rc;

	if (!topology || !groups || group_count <= 0 || target_group < 0 ||
	    target_group >= group_count || group_count + 1 > 16)
		return FABRIC_ERROR;

	fabric_global_reset();

	source = create_running_fabric();
	if (!source)
		return FABRIC_ERROR;

	topology->name = name;
	topology->source = source;
	topology->target = (void *) 0;
	topology->node_count = 0;
	topology->nodes[topology->node_count++] = source;

	for (group_index = 0; group_index < group_count; group_index++) {
		node = create_running_fabric();
		if (!node)
			return FABRIC_ERROR;

		topology->nodes[topology->node_count++] = node;

		for (component_index = 0;
		     component_index < groups[group_index].count;
		     component_index++) {
			rc = register_symbol(node,
					     groups[group_index].components[component_index].name,
					     groups[group_index].components[component_index].func);
			if (rc != FABRIC_SUCCESS)
				return rc;
		}

		rc = fabric_bind_bidi(source, node);
		if (rc != FABRIC_SUCCESS)
			return rc;

		if (group_index == target_group)
			topology->target = node;
	}

	/* Attach MPK after the full topology exists so every fabric has a key. */
	for (group_index = 0; group_index < topology->node_count; group_index++) {
		rc = enable_mpk_isolation(topology->nodes[group_index]);
		if (rc != FABRIC_SUCCESS) {
			flexos_nop_gate(0, 0, printf,
					"[extension-overhead] MPK init failed on fabric %u\n",
					topology->nodes[group_index]->fabric_id);
			return rc;
		}
	}

	flexos_nop_gate(0, 0, printf,
			"[extension-overhead] %s: fabrics=%d domains=%d MPK=on\n",
			name, topology->node_count, group_count);

	return topology->target ? FABRIC_SUCCESS : FABRIC_ERROR;
}

static uint64_t measure_partition_overhead(uint64_t empty_overhead,
					 struct fabric_topology *topology)
{
	uint64_t min_cycles = UINT64_MAX;
	uint64_t t0;
	uint64_t t1;
	int i;
	uint32_t source_id;

	if (!topology || !topology->source)
		return UINT64_MAX;

	source_id = topology->source->fabric_id;

	/* Resolve example8_empty via DFS+map; MPK enter/leave on cross-fabric. */
	for (i = 0; i < REPS; i++) {
		t0 = bench_start();
		fabric_gate(source_id, example8_empty);
		t1 = bench_end();
		if ((t1 - t0) < min_cycles)
			min_cycles = t1 - t0;
	}

	return min_cycles - empty_overhead;
}

static int run_partition_benchmark(const char *label,
				   const struct component_group groups[],
				   int group_count,
				   int target_group)
{
	struct fabric_topology topology;
	uint64_t empty_overhead;
	uint64_t partition_overhead;

	/*
	 * ukboot leaves PKRU = key0-only (0x3ffffffc). Comp1 .bss/.data
	 * (newlib stdout, etc.) are tagged key 1, so open key0+key1 before
	 * any printf. Akira Fabric MPK uses keys 2..15.
	 */
#if CONFIG_LIBFLEXOS_INTELPKU
	wrpkru(0x3ffffff0);
#endif

	console_marker("[extension-overhead] entered benchmark\n");
	flexos_nop_gate(0, 0, printf,
			"Starting %s benchmark...\n", label);

	if (build_partitioned_topology(&topology, label, groups,
				      group_count, target_group) != FABRIC_SUCCESS) {
		flexos_nop_gate(0, 0, printf,
				"failed to initialize %s topology\n", label);
		return 1;
	}

	empty_overhead = measure_empty_overhead();
	partition_overhead = measure_partition_overhead(empty_overhead, &topology);

	flexos_nop_gate(0, 0, printf,
			"empty_overhead: %" PRIu64 " cycles\n", empty_overhead);
	flexos_nop_gate(0, 0, printf,
			"%s: %" PRIu64 " cycles\n", label, partition_overhead);

	return 0;
}

#endif
