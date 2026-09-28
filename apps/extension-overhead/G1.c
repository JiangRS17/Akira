/* SPDX-License-Identifier: BSD-3-Clause */

#include "gbench_common.h"

static const struct bench_component g1_domain0[] = {
	{ "example1_empty", (func_t) example1_empty },
	{ "example2_empty", (func_t) example2_empty },
	{ "example3_empty", (func_t) example3_empty },
	{ "example4_empty", (func_t) example4_empty },
};

static const struct bench_component g1_domain1[] = {
	{ "example5_empty", (func_t) example5_empty },
	{ "example6_empty", (func_t) example6_empty },
	{ "example7_empty", (func_t) example7_empty },
	{ "example8_empty", (func_t) example8_empty },
};

static const struct component_group g1_groups[] = {
	{ g1_domain0, 4 },
	{ g1_domain1, 4 },
};

int main(void)
{
	return run_partition_benchmark(
		"G1_8_components_2_domains",
		g1_groups,
		2,
		1);
}