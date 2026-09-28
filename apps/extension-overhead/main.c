// /* SPDX-License-Identifier: BSD-3-Clause */

// #include <inttypes.h>
// #include <stdint.h>
// #include <stdio.h>

// #include <fabric/bridge.h>
// #include <fabric/component.h>
// #include <fabric/fabric.h>
// #include <fabric/fabric_error.h>
// #include <fabric/impl/fabric_gate.h>
// #include <flexos/isolation.h>
// #include <uk/plat/console.h>

// #include <example1/isolated.h>
// #include <example2/isolated.h>
// #include <example3/isolated.h>
// #include <example4/isolated.h>
// #include <example5/isolated.h>
// #include <example6/isolated.h>
// #include <example7/isolated.h>
// #include <example8/isolated.h>

// /*
//  * Use a moderate iteration count so the full benchmark suite finishes in a
//  * practical amount of time while still smoothing one-off timing noise.
//  */
// #define REPS 1000000

// struct fabric_topology {
// 	const char *name;
// 	Fabric *source;
// 	Fabric *target;
// };

// struct domain_group {
// 	const char *const *names;
// 	func_t *funcs;
// 	int count;
// };

// int fabric_bridge_bind_downstream(struct FabricBridge *bridge, Fabric *fabric);

// __attribute__((always_inline)) static inline uint64_t bench_start(void)
// {
// 	unsigned cycles_low;
// 	unsigned cycles_high;

// 	asm volatile(
// 		"CPUID\n\t"
// 		"RDTSC\n\t"
// 		"MOV %%edx, %0\n\t"
// 		"MOV %%eax, %1\n\t"
// 		: "=r" (cycles_high), "=r" (cycles_low)
// 		:
// 		: "%rax", "%rbx", "%rcx", "%rdx");

// 	return ((uint64_t) cycles_high << 32) | cycles_low;
// }

// __attribute__((always_inline)) static inline uint64_t bench_end(void)
// {
// 	unsigned cycles_low;
// 	unsigned cycles_high;

// 	asm volatile(
// 		"RDTSCP\n\t"
// 		"MOV %%edx, %0\n\t"
// 		"MOV %%eax, %1\n\t"
// 		"CPUID\n\t"
// 		: "=r" (cycles_high), "=r" (cycles_low)
// 		:
// 		: "%rax", "%rbx", "%rcx", "%rdx");

// 	return ((uint64_t) cycles_high << 32) | cycles_low;
// }

// __attribute__((noinline))
// static void empty_fcall(void)
// {
// 	asm volatile("");
// }

// static uint64_t measure_empty_overhead(void)
// {
// 	uint64_t min_cycles = UINT64_MAX;
// 	uint64_t t0;
// 	uint64_t t1;
// 	int i;

// 	for (i = 0; i < REPS; i++) {
// 		t0 = bench_start();
// 		asm volatile("");
// 		t1 = bench_end();
// 		if ((t1 - t0) < min_cycles)
// 			min_cycles = t1 - t0;
// 	}

// 	return min_cycles;
// }

// static uint64_t measure_direct_fcall(uint64_t empty_overhead)
// {
// 	uint64_t min_cycles = UINT64_MAX;
// 	uint64_t t0;
// 	uint64_t t1;
// 	int i;

// 	for (i = 0; i < REPS; i++) {
// 		t0 = bench_start();
// 		empty_fcall();
// 		t1 = bench_end();
// 		if ((t1 - t0) < min_cycles)
// 			min_cycles = t1 - t0;
// 	}

// 	return min_cycles - empty_overhead;
// }

// /* G0 baseline: same compartment, no Fabric, single logical domain. */
// static uint64_t measure_g0_same_domain(uint64_t empty_overhead)
// {
// 	uint64_t min_cycles = UINT64_MAX;
// 	uint64_t t0;
// 	uint64_t t1;
// 	int i;

// 	for (i = 0; i < REPS; i++) {
// 		t0 = bench_start();
// 		flexos_nop_gate(0, 0, example1_empty);
// 		t1 = bench_end();
// 		if ((t1 - t0) < min_cycles)
// 			min_cycles = t1 - t0;
// 	}

// 	return min_cycles - empty_overhead;
// }

// /* E1 baseline: one FlexOS call crossing from comp1 to comp2. */
// static uint64_t measure_e1_cross_compartment(uint64_t empty_overhead)
// {
// 	uint64_t min_cycles = UINT64_MAX;
// 	uint64_t t0;
// 	uint64_t t1;
// 	int i;

// 	for (i = 0; i < REPS; i++) {
// 		t0 = bench_start();
// 		flexos_intelpku_gate(0, 1, example2_empty);
// 		t1 = bench_end();
// 		if ((t1 - t0) < min_cycles)
// 			min_cycles = t1 - t0;
// 	}

// 	return min_cycles - empty_overhead;
// }

// static int fabric_bind_bidi(Fabric *left, Fabric *right)
// {
// 	if (!left || !right || !left->bridge || !right->bridge)
// 		return FABRIC_ERROR;
// 	if (fabric_bridge_bind_downstream(left->bridge, right) != FABRIC_SUCCESS)
// 		return FABRIC_ERROR;
// 	if (fabric_bridge_bind_downstream(right->bridge, left) != FABRIC_SUCCESS)
// 		return FABRIC_ERROR;
// 	return FABRIC_SUCCESS;
// }

// static Fabric *create_running_fabric(void)
// {
// 	Fabric *fabric = fabric_create();

// 	if (!fabric || !fabric->component)
// 		return (void *) 0;

// 	fabric->component->status = FB_COMP_STATE_RUNNING;
// 	return fabric;
// }

// static int register_symbol(Fabric *fabric, const char *name, func_t fn)
// {
// 	return fabric_register_function(fabric, name, fn);
// }

// static void console_marker(const char *msg)
// {
// 	ukplat_coutk(msg, 0);
// }

// /*
//  * Keep the component set fixed across all scalability tests.
//  * G1/G2/G3 only change how the same four components are partitioned into
//  * Fabric domains:
//  * - G1: all components in one Fabric domain
//  * - G2: the same components split across two Fabric domains
//  * - G3: one component per Fabric domain
//  *
//  * The source domain is a fixed caller domain and is not counted as part of
//  * the partitioning granularity.
//  */
// static int build_partitioned_topology(struct fabric_topology *topology,
// 				      const char *name,
// 				      const struct domain_group groups[],
// 				      int group_count,
// 				      int target_group)
// {
// 	Fabric *source;
// 	Fabric *node;
// 	int rc;
// 	int group_index;
// 	int i;

// 	if (!topology || !groups || group_count <= 0 || target_group < 0 ||
// 	    target_group >= group_count)
// 		return FABRIC_ERROR;

// 	source = create_running_fabric();
// 	if (!source)
// 		return FABRIC_ERROR;

// 	for (group_index = 0; group_index < group_count; group_index++) {
// 		node = create_running_fabric();
// 		if (!node)
// 			return FABRIC_ERROR;

// 		for (i = 0; i < groups[group_index].count; i++) {
// 			rc = register_symbol(node,
// 					     groups[group_index].names[i],
// 					     groups[group_index].funcs[i]);
// 			if (rc != FABRIC_SUCCESS)
// 				return rc;
// 		}

// 		rc = fabric_bind_bidi(source, node);
// 		if (rc != FABRIC_SUCCESS)
// 			return rc;

// 		if (group_index == target_group)
// 			topology->target = node;
// 	}

// 	topology->name = name;
// 	topology->source = source;
// 	return FABRIC_SUCCESS;
// }

// static int init_g1_topology(struct fabric_topology *topology)
// {
// 	static const char *const group0_names[] = {
// 		"example3_empty",
// 		"example4_empty",
// 		"example5_empty",
// 		"example6_empty",
// 	};
// 	static func_t group0_funcs[] = {
// 		(func_t) example3_empty,
// 		(func_t) example4_empty,
// 		(func_t) example5_empty,
// 		(func_t) example6_empty,
// 	};
// 	static const struct domain_group groups[] = {
// 		{ group0_names, group0_funcs, 4 },
// 	};

// 	return build_partitioned_topology(topology, "G1", groups, 1, 0);
// }

// static int init_g2_topology(struct fabric_topology *topology)
// {
// 	static const char *const group0_names[] = {
// 		"example4_empty",
// 		"example5_empty",
// 	};
// 	static const char *const group1_names[] = {
// 		"example3_empty",
// 		"example6_empty",
// 	};
// 	static func_t group0_funcs[] = {
// 		(func_t) example4_empty,
// 		(func_t) example5_empty,
// 	};
// 	static func_t group1_funcs[] = {
// 		(func_t) example3_empty,
// 		(func_t) example6_empty,
// 	};
// 	static const struct domain_group groups[] = {
// 		{ group0_names, group0_funcs, 2 },
// 		{ group1_names, group1_funcs, 2 },
// 	};

// 	return build_partitioned_topology(topology, "G2", groups, 2, 1);
// }

// static int init_g3_topology(struct fabric_topology *topology)
// {
// 	static const char *const group0_names[] = { "example4_empty" };
// 	static const char *const group1_names[] = { "example5_empty" };
// 	static const char *const group2_names[] = { "example6_empty" };
// 	static const char *const group3_names[] = { "example3_empty" };
// 	static func_t group0_funcs[] = { (func_t) example4_empty };
// 	static func_t group1_funcs[] = { (func_t) example5_empty };
// 	static func_t group2_funcs[] = { (func_t) example6_empty };
// 	static func_t group3_funcs[] = { (func_t) example3_empty };
// 	static const struct domain_group groups[] = {
// 		{ group0_names, group0_funcs, 1 },
// 		{ group1_names, group1_funcs, 1 },
// 		{ group2_names, group2_funcs, 1 },
// 		{ group3_names, group3_funcs, 1 },
// 	};

// 	return build_partitioned_topology(topology, "G3", groups, 4, 3);
// }

// static uint64_t measure_fabric_scaling(uint64_t empty_overhead,
// 				      struct fabric_topology *topology)
// {
// 	uint64_t min_cycles = UINT64_MAX;
// 	uint64_t t0;
// 	uint64_t t1;
// 	int i;

// 	if (!topology || !topology->source)
// 		return UINT64_MAX;

// 	for (i = 0; i < REPS; i++) {
// 		t0 = bench_start();
// 		fabric_gate(topology->source->fabric_id, topology->source->fabric_id, example3_empty);
// 		t1 = bench_end();
// 		if ((t1 - t0) < min_cycles)
// 			min_cycles = t1 - t0;
// 	}

// 	return min_cycles - empty_overhead;
// }

// static int init_all_topologies(struct fabric_topology *g1,
// 			       struct fabric_topology *g2,
// 			       struct fabric_topology *g3)
// {
// 	if (init_g1_topology(g1) != FABRIC_SUCCESS)
// 		return FABRIC_ERROR;
// 	if (init_g2_topology(g2) != FABRIC_SUCCESS)
// 		return FABRIC_ERROR;
// 	if (init_g3_topology(g3) != FABRIC_SUCCESS)
// 		return FABRIC_ERROR;
// 	return FABRIC_SUCCESS;
// }

// int main(void)
// {	
// 	console_marker("[extension-overhead] entered main\n");
// 	flexos_nop_gate(0, 0, printf,
// 			"Starting extension-overhead benchmarks...\n");
// 	struct fabric_topology g1;
// 	struct fabric_topology g2;
// 	struct fabric_topology g3;
// 	uint64_t empty_overhead;
// 	uint64_t direct_empty_fcall;
// 	uint64_t g0_same_domain;
// 	uint64_t e1_cross_compartment;
// 	uint64_t g1_coarse;
// 	uint64_t g2_medium;
// 	uint64_t g3_fine;

// 	flexos_nop_gate(0, 0, printf,
// 			"Starting extension-overhead benchmarks...\n");

// 	console_marker("[extension-overhead] initializing topologies\n");
// 	if (init_all_topologies(&g1, &g2, &g3) != FABRIC_SUCCESS) {
// 		console_marker("[extension-overhead] topology init failed\n");
// 		flexos_nop_gate(0, 0, printf,
// 				"failed to initialize Fabric scaling topologies\n");
// 		return 1;
// 	}
// 	console_marker("[extension-overhead] topologies ready\n");

// 	empty_overhead = measure_empty_overhead();
// 	console_marker("[extension-overhead] empty overhead done\n");
// 	direct_empty_fcall = measure_direct_fcall(empty_overhead);
// 	g0_same_domain = measure_g0_same_domain(empty_overhead);
// 	e1_cross_compartment = measure_e1_cross_compartment(empty_overhead);
// 	console_marker("[extension-overhead] base baselines done\n");

// 	/*
// 	 * G1/G2/G3 always use the same component set: example3-6.
// 	 * Only the partitioning of these components across Fabric domains changes.
// 	 */
// 	g1_coarse = measure_fabric_scaling(empty_overhead, &g1);
// 	console_marker("[extension-overhead] g1 done\n");
// 	g2_medium = measure_fabric_scaling(empty_overhead, &g2);
// 	console_marker("[extension-overhead] g2 done\n");
// 	g3_fine = measure_fabric_scaling(empty_overhead, &g3);
// 	console_marker("[extension-overhead] g3 done\n");

// 	/* E2 is the same mechanism as G1: same-compartment Fabric over 2 domains. */
// 	flexos_nop_gate(0, 0, printf,
// 			"empty_overhead: %" PRIu64 " cycles\n", empty_overhead);
// 	flexos_nop_gate(0, 0, printf,
// 			"E0_direct_empty_fcall: %" PRIu64 " cycles\n",
// 			direct_empty_fcall);
// 	flexos_nop_gate(0, 0, printf,
// 			"E1_flexos_cross_compartment: %" PRIu64 " cycles\n",
// 			e1_cross_compartment);
// 	flexos_nop_gate(0, 0, printf,
// 			"G0_same_domain_no_fabric: %" PRIu64 " cycles\n",
// 			g0_same_domain);
// 	flexos_nop_gate(0, 0, printf,
// 			"G1_all_components_1_domain: %" PRIu64 " cycles\n",
// 			g1_coarse);
// 	flexos_nop_gate(0, 0, printf,
// 			"G2_all_components_2_domains: %" PRIu64 " cycles\n",
// 			g2_medium);
// 	flexos_nop_gate(0, 0, printf,
// 			"G3_each_component_1_domain: %" PRIu64 " cycles\n",
// 			g3_fine);

// 	return 0;
// }
