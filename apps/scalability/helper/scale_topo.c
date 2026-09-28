/* SPDX-License-Identifier: BSD-3-Clause */
#include "scale_topo.h"

#include <bridge.h>
#include <fabric_error.h>
#include <backends.h>
#include <flexos/isolation.h>
#include <uk/alloc.h>
#include <stdio.h>
#include <string.h>

static void scale_empty(void)
{
	asm volatile("");
}

static int bind_bidi(Fabric *a, Fabric *b)
{
	if (!a || !b || !a->bridge || !b->bridge)
		return FABRIC_ERROR;
	if (fabric_bridge_bind_downstream(a->bridge, b) != FABRIC_SUCCESS)
		return FABRIC_ERROR;
	if (fabric_bridge_bind_downstream(b->bridge, a) != FABRIC_SUCCESS)
		return FABRIC_ERROR;
	return FABRIC_SUCCESS;
}

/* ---------------- chain (layers) ---------------- */

int scale_chain_build(struct scale_chain *chain, unsigned max_hops)
{
	unsigned i;
	MPKConfig iso_cfg;

	if (!chain || max_hops < 1 || max_hops > SCALE_MAX_HOPS)
		return FABRIC_ERROR;

	fabric_global_reset();
	akira_mpk_key_reset();
	memset(chain, 0, sizeof(*chain));
	chain->num_fabrics = max_hops + 1;

	for (i = 0; i < chain->num_fabrics; i++) {
		chain->fabrics[i] = fabric_create();
		if (!chain->fabrics[i])
			goto fail;
	}

	for (i = 0; i + 1 < chain->num_fabrics; i++) {
		if (bind_bidi(chain->fabrics[i], chain->fabrics[i + 1]) !=
		    FABRIC_SUCCESS)
			goto fail;
	}

	for (i = 1; i < chain->num_fabrics; i++) {
		snprintf(chain->svc_names[i], sizeof(chain->svc_names[i]),
			 "L%02u", i);
		if (fabric_register_function(chain->fabrics[i],
					     chain->svc_names[i],
					     (func_t)scale_empty) !=
		    FABRIC_SUCCESS)
			goto fail;
	}

	for (i = 0; i < chain->num_fabrics; i++) {
		iso_cfg.fabric_id = chain->fabrics[i]->fabric_id;
		if (fabric_bridge_set_isolation(chain->fabrics[i]->bridge,
						FABRIC_ISO_MPK,
						&iso_cfg) != FABRIC_SUCCESS ||
		    !chain->fabrics[i]->bridge->iso_state) {
			printf("[scale-topo] chain: MPK init fail fabric %u "
			       "(key budget?)\n",
			       i);
			goto fail;
		}
	}

	printf("[scale-topo] chain fabrics=%u max_hops=%u (MPK-all)\n",
	       chain->num_fabrics, max_hops);
	return FABRIC_SUCCESS;

fail:
	scale_chain_destroy(chain);
	fabric_global_reset();
	akira_mpk_key_reset();
	return FABRIC_ERROR;
}

void scale_chain_destroy(struct scale_chain *chain)
{
	unsigned i;

	if (!chain)
		return;
	for (i = chain->num_fabrics; i > 0; i--) {
		if (chain->fabrics[i - 1]) {
			fabric_destroy(chain->fabrics[i - 1]);
			chain->fabrics[i - 1] = NULL;
		}
	}
	chain->num_fabrics = 0;
}

const char *scale_chain_svc(const struct scale_chain *chain, unsigned hops)
{
	if (!chain || hops < 1 || hops >= chain->num_fabrics)
		return NULL;
	return chain->svc_names[hops];
}

/* ---------------- star (library count) ---------------- */

int scale_star_build(struct scale_star *star, unsigned n_libs)
{
	return scale_star_build_iso(star, n_libs, SCALE_ISO_ALL_MPK);
}

int scale_star_build_iso(struct scale_star *star, unsigned n_libs, int iso_mode)
{
	unsigned i;

	if (!star || n_libs < 2 || n_libs > SCALE_MAX_LIBS) {
		printf("[scale-topo] star: bad n_libs=%u (max %u)\n",
		       n_libs, SCALE_MAX_LIBS);
		return FABRIC_ERROR;
	}
	if (iso_mode != SCALE_ISO_ALL_MPK && iso_mode != SCALE_ISO_PATH_MPK) {
		printf("[scale-topo] star: bad iso_mode=%d\n", iso_mode);
		return FABRIC_ERROR;
	}
	if (n_libs - 1 > FABRIC_MAX_DOWNSTREAM) {
		printf("[scale-topo] star: leaves %u > MAX_DOWNSTREAM %u\n",
		       n_libs - 1, FABRIC_MAX_DOWNSTREAM);
		return FABRIC_ERROR;
	}

	fabric_global_reset();
	akira_mpk_key_reset();
	memset(star, 0, sizeof(*star));
	star->n_libs = n_libs;
	star->n_leaves = n_libs - 1;
	star->iso_mode = iso_mode;
	snprintf(star->target_svc, sizeof(star->target_svc), "LIB");

	star->hub = fabric_create();
	if (!star->hub) {
		printf("[scale-topo] star: hub alloc failed\n");
		goto fail;
	}

	for (i = 0; i < star->n_leaves; i++) {
		star->leaves[i] = fabric_create();
		if (!star->leaves[i]) {
			printf("[scale-topo] star: leaf %u alloc failed\n", i);
			goto fail;
		}
		if (bind_bidi(star->hub, star->leaves[i]) != FABRIC_SUCCESS) {
			printf("[scale-topo] star: bind leaf %u failed\n", i);
			goto fail;
		}
	}

	/*
	 * Register a service on every leaf so domain-RR can land on any leaf;
	 * leaf[0] also keeps shared name "LIB" for fixed-target calls.
	 */
	for (i = 0; i < star->n_leaves; i++) {
		char lname[8];

		snprintf(lname, sizeof(lname), "L%02u", i);
		if (fabric_register_function(star->leaves[i], lname,
					     (func_t)scale_empty) !=
		    FABRIC_SUCCESS) {
			printf("[scale-topo] star: register leaf %u failed\n",
			       i);
			goto fail;
		}
	}
	if (fabric_register_function(star->leaves[0], star->target_svc,
				     (func_t)scale_empty) != FABRIC_SUCCESS) {
		printf("[scale-topo] star: register LIB failed\n");
		goto fail;
	}

	/* MPK isolation: all components, or path (hub + target) only. */
	{
		MPKConfig iso_cfg;

		iso_cfg.fabric_id = star->hub->fabric_id;
		if (fabric_bridge_set_isolation(star->hub->bridge, FABRIC_ISO_MPK,
						&iso_cfg) != FABRIC_SUCCESS ||
		    !star->hub->bridge->iso_state) {
			printf("[scale-topo] star: MPK init fail hub\n");
			goto fail;
		}

		if (iso_mode == SCALE_ISO_ALL_MPK) {
			for (i = 0; i < star->n_leaves; i++) {
				iso_cfg.fabric_id = star->leaves[i]->fabric_id;
				if (fabric_bridge_set_isolation(
					    star->leaves[i]->bridge,
					    FABRIC_ISO_MPK, &iso_cfg) !=
					    FABRIC_SUCCESS ||
				    !star->leaves[i]->bridge->iso_state) {
					printf("[scale-topo] star: MPK init "
					       "fail leaf %u (key budget?)\n",
					       i);
					goto fail;
				}
			}
			star->keys_used = n_libs;
		} else {
			/* Path only: target leaf; siblings stay route-only. */
			iso_cfg.fabric_id = star->leaves[0]->fabric_id;
			if (fabric_bridge_set_isolation(
				    star->leaves[0]->bridge, FABRIC_ISO_MPK,
				    &iso_cfg) != FABRIC_SUCCESS ||
			    !star->leaves[0]->bridge->iso_state) {
				printf("[scale-topo] star: MPK init fail target\n");
				goto fail;
			}
			star->keys_used = 2;
		}
	}

	printf("[scale-topo] star n_libs=%u leaves=%u keys=%u (%s)\n",
	       star->n_libs, star->n_leaves, star->keys_used,
	       iso_mode == SCALE_ISO_ALL_MPK ? "MPK-all" : "MPK-path");
	return FABRIC_SUCCESS;

fail:
	scale_star_destroy(star);
	fabric_global_reset();
	akira_mpk_key_reset();
	return FABRIC_ERROR;
}

void scale_star_destroy(struct scale_star *star)
{
	unsigned i;

	if (!star)
		return;
	for (i = star->n_leaves; i > 0; i--) {
		if (star->leaves[i - 1]) {
			fabric_destroy(star->leaves[i - 1]);
			star->leaves[i - 1] = NULL;
		}
	}
	if (star->hub) {
		fabric_destroy(star->hub);
		star->hub = NULL;
	}
	star->n_libs = 0;
	star->n_leaves = 0;
	star->iso_mode = 0;
	star->keys_used = 0;
}

const char *scale_star_target_svc(const struct scale_star *star)
{
	if (!star || star->n_leaves < 1)
		return NULL;
	return star->target_svc;
}

uint32_t scale_star_leaf_id(const struct scale_star *star, unsigned leaf_idx)
{
	if (!star || leaf_idx >= star->n_leaves || !star->leaves[leaf_idx])
		return (uint32_t)-1;
	return star->leaves[leaf_idx]->fabric_id;
}

unsigned scale_star_keys_used(const struct scale_star *star)
{
	return star ? star->keys_used : 0;
}

/* ---------------- fat chain / grid ---------------- */

static Fabric *grid_at(const struct scale_grid *g, unsigned layer, unsigned i)
{
	return g->nodes[layer * g->libs_per_layer + i];
}

int scale_grid_build(struct scale_grid *g, unsigned n_hops,
		     unsigned libs_per_layer)
{
	unsigned layer, i;
	unsigned n_layers, total;
	size_t bytes;

	if (!g || n_hops < 1 || n_hops > SCALE_MAX_LAYERS)
		return FABRIC_ERROR;
	if (libs_per_layer < 2 || libs_per_layer > SCALE_MAX_PER_LAYER)
		return FABRIC_ERROR;

	n_layers = n_hops + 1;
	total = n_layers * libs_per_layer;
	if (total > MAX_FABRIC) {
		printf("[scale-topo] grid: need %u fabrics > MAX_FABRIC %u\n",
		       total, MAX_FABRIC);
		return FABRIC_ERROR;
	}
	/* hub fan-out: next/prev chain + (K-1) siblings */
	if (libs_per_layer + 1 > FABRIC_MAX_DOWNSTREAM) {
		printf("[scale-topo] grid: fan-out too large for K=%u\n",
		       libs_per_layer);
		return FABRIC_ERROR;
	}

	fabric_global_reset();
	akira_mpk_key_reset();
	fabric_create_skip_queues = 1;
	memset(g, 0, sizeof(*g));
	g->n_hops = n_hops;
	g->libs_per_layer = libs_per_layer;
	g->n_layers = n_layers;
	snprintf(g->target_svc, sizeof(g->target_svc), "TGT");

	bytes = (size_t)total * sizeof(Fabric *);
	g->nodes = uk_zalloc(flexos_shared_alloc, bytes);
	if (!g->nodes) {
		printf("[scale-topo] grid: nodes[] alloc failed (%u)\n", total);
		return FABRIC_ERROR;
	}

	for (layer = 0; layer < n_layers; layer++) {
		for (i = 0; i < libs_per_layer; i++) {
			int is_dst = (layer == n_hops && i == 0);
			Fabric *f;

			/* Only destination hub executes; others are route-only. */
			fabric_create_skip_component = is_dst ? 0 : 1;
			f = fabric_create();
			fabric_create_skip_component = 0;

			if (!f) {
				printf("[scale-topo] grid: fabric L%u[%u] fail\n",
				       layer, i);
				goto fail;
			}
			g->nodes[layer * libs_per_layer + i] = f;
		}
	}

	/*
	 * Next-hop first, then siblings. DFS walks downstream[] in order
	 * (forward-id pass), so the path hub is tried before scanning K-1
	 * unused libraries — latency ~O(L), flat in K (good scalability).
	 */
	for (layer = 0; layer < n_hops; layer++) {
		if (bind_bidi(grid_at(g, layer, 0),
			      grid_at(g, layer + 1, 0)) != FABRIC_SUCCESS) {
			printf("[scale-topo] grid: chain bind L%u failed\n",
			       layer);
			goto fail;
		}
	}
	for (layer = 0; layer < n_layers; layer++) {
		Fabric *hub = grid_at(g, layer, 0);

		for (i = 1; i < libs_per_layer; i++) {
			if (bind_bidi(hub, grid_at(g, layer, i)) !=
			    FABRIC_SUCCESS) {
				printf("[scale-topo] grid: sib L%u[%u] fail\n",
				       layer, i);
				goto fail;
			}
		}
	}

	if (fabric_register_function(grid_at(g, n_hops, 0), g->target_svc,
				     (func_t)scale_empty) != FABRIC_SUCCESS) {
		printf("[scale-topo] grid: register failed\n");
		goto fail;
	}

	/*
	 * MPK on path hubs only (L+1 ≤ 6 keys). Sibling libraries stay
	 * route-only without keys so K can scale to thousands.
	 */
	for (layer = 0; layer < n_layers; layer++) {
		Fabric *hub = grid_at(g, layer, 0);
		MPKConfig iso_cfg;

		iso_cfg.fabric_id = hub->fabric_id;
		if (fabric_bridge_set_isolation(hub->bridge, FABRIC_ISO_MPK,
						&iso_cfg) != FABRIC_SUCCESS ||
		    !hub->bridge->iso_backend || !hub->bridge->iso_state) {
			printf("[scale-topo] grid: MPK init fail L%u hub\n",
			       layer);
			goto fail;
		}
	}

	printf("[scale-topo] grid hops=%u K=%u fabrics=%u "
	       "(MPK hubs=%u, nexthop-before-sib)\n",
	       n_hops, libs_per_layer, total, n_layers);
	return FABRIC_SUCCESS;

fail:
	scale_grid_destroy(g);
	fabric_global_reset();
	return FABRIC_ERROR;
}

void scale_grid_destroy(struct scale_grid *g)
{
	unsigned total, i;

	if (!g)
		return;
	if (g->nodes) {
		total = g->n_layers * g->libs_per_layer;
		for (i = total; i > 0; i--) {
			if (g->nodes[i - 1]) {
				fabric_destroy(g->nodes[i - 1]);
				g->nodes[i - 1] = NULL;
			}
		}
		uk_free(flexos_shared_alloc, g->nodes);
		g->nodes = NULL;
	}
	g->n_hops = 0;
	g->libs_per_layer = 0;
	g->n_layers = 0;
}

Fabric *scale_grid_src(const struct scale_grid *g)
{
	if (!g || !g->nodes)
		return NULL;
	return grid_at(g, 0, 0);
}

const char *scale_grid_target_svc(const struct scale_grid *g)
{
	if (!g || !g->nodes)
		return NULL;
	return g->target_svc;
}

/* ---------------- service map fill ---------------- */

#include <controller.h>
#include <map.h>

int scale_map_recreate(Fabric *fabric, uint32_t capacity)
{
	FabricFunctionMap *neu;

	if (!fabric || !fabric->controller)
		return FABRIC_ERROR;
	if (capacity < 4)
		capacity = 4;

	neu = function_map_create(capacity);
	if (!neu)
		return FABRIC_ERROR;

	if (fabric->controller->function_map)
		function_map_destroy(fabric->controller->function_map);
	fabric->controller->function_map = neu;
	return FABRIC_SUCCESS;
}

int scale_map_fill(Fabric *fabric, unsigned n_entries, unsigned target_idx,
		   char *out_name, unsigned out_len)
{
	unsigned i;
	char name[16];

	if (!fabric || n_entries < 1 || n_entries > SCALE_MAX_MAP_ENTRIES)
		return FABRIC_ERROR;
	if (target_idx >= n_entries)
		return FABRIC_ERROR;

	for (i = 0; i < n_entries; i++) {
		/* Equal-length names → comparable FNV cost. */
		snprintf(name, sizeof(name), "S%06u", i);
		if (fabric_register_function(fabric, name,
					     (func_t)scale_empty) !=
		    FABRIC_SUCCESS)
			return FABRIC_ERROR;
	}

	if (out_name && out_len > 0) {
		snprintf(out_name, out_len, "S%06u", target_idx);
	}
	return FABRIC_SUCCESS;
}
