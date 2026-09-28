/* SPDX-License-Identifier: BSD-3-Clause */

#include "gbench_common.h"

static const struct bench_component g2_domain0[] = {
	{ "example1_empty", (func_t) example1_empty },
	{ "example2_empty", (func_t) example2_empty },
};

static const struct bench_component g2_domain1[] = {
	{ "example3_empty", (func_t) example3_empty },
	{ "example4_empty", (func_t) example4_empty },
};

static const struct bench_component g2_domain2[] = {
	{ "example5_empty", (func_t) example5_empty },
	{ "example6_empty", (func_t) example6_empty },
};

static const struct bench_component g2_domain3[] = {
	{ "example7_empty", (func_t) example7_empty },
	{ "example8_empty", (func_t) example8_empty },
};

static const struct component_group g2_groups[] = {
	{ g2_domain0, 2 },
	{ g2_domain1, 2 },
	{ g2_domain2, 2 },
	{ g2_domain3, 2 },
};

int main(void)
{
	return run_partition_benchmark(
		"G2_8_components_4_domains",
		g2_groups,
		4,
		3);
}