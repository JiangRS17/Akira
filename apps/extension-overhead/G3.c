/* SPDX-License-Identifier: BSD-3-Clause */

#include "gbench_common.h"

static const struct bench_component g3_domain0[] = {
	{ "example1_empty", (func_t) example1_empty },
};

static const struct bench_component g3_domain1[] = {
	{ "example2_empty", (func_t) example2_empty },
};

static const struct bench_component g3_domain2[] = {
	{ "example3_empty", (func_t) example3_empty },
};

static const struct bench_component g3_domain3[] = {
	{ "example4_empty", (func_t) example4_empty },
};

static const struct bench_component g3_domain4[] = {
	{ "example5_empty", (func_t) example5_empty },
};

static const struct bench_component g3_domain5[] = {
	{ "example6_empty", (func_t) example6_empty },
};

static const struct bench_component g3_domain6[] = {
	{ "example7_empty", (func_t) example7_empty },
};

static const struct bench_component g3_domain7[] = {
	{ "example8_empty", (func_t) example8_empty },
};

static const struct component_group g3_groups[] = {
	{ g3_domain0, 1 },
	{ g3_domain1, 1 },
	{ g3_domain2, 1 },
	{ g3_domain3, 1 },
	{ g3_domain4, 1 },
	{ g3_domain5, 1 },
	{ g3_domain6, 1 },
	{ g3_domain7, 1 },
};

int main(void)
{
	return run_partition_benchmark(
		"G3_8_components_8_domains",
		g3_groups,
		8,
		7);
}