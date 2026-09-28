/* SPDX-License-Identifier: BSD-3-Clause */

#include "./include/overhead.h"

static struct fabric_overhead_stats g_stats;
static int g_enabled;
static uint32_t g_ablation;

void fabric_overhead_reset(void)
{
	g_stats = (struct fabric_overhead_stats){0};
}

void fabric_overhead_enable(int on)
{
	g_enabled = on ? 1 : 0;
}

int fabric_overhead_enabled(void)
{
	return g_enabled;
}

void fabric_overhead_get(struct fabric_overhead_stats *out)
{
	if (out)
		*out = g_stats;
}

void fabric_ablation_set(uint32_t flags)
{
	g_ablation = flags;
}

uint32_t fabric_ablation_get(void)
{
	return g_ablation;
}

struct fabric_overhead_stats *fabric_overhead_stats_ptr(void)
{
	return &g_stats;
}

void fabric_dfs_visit_add(uint64_t n)
{
	g_stats.dfs_visits += n;
}
