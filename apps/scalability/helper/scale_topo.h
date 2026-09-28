/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef SCALE_TOPO_H
#define SCALE_TOPO_H

#include <fabric.h>
#include <stdint.h>

/*
 * Soft-bus layer bench (chain). Sized for MPK domain sweep (≤14 usable keys).
 */
#define SCALE_MAX_HOPS		14
#define SCALE_MAX_CHAIN_FABRICS	(SCALE_MAX_HOPS + 1)

struct scale_chain {
	Fabric *fabrics[SCALE_MAX_CHAIN_FABRICS];
	unsigned num_fabrics;
	char svc_names[SCALE_MAX_CHAIN_FABRICS][8];
};

int scale_chain_build(struct scale_chain *chain, unsigned max_hops);
void scale_chain_destroy(struct scale_chain *chain);
const char *scale_chain_svc(const struct scale_chain *chain, unsigned hops);

/*
 * Star: hub + leaves (1-hop to first leaf).
 */
#define SCALE_MAX_LIBS		512

/*
 * MPK: key0 shared, key1 FlexOS default → Akira uses keys 2..15 (14 domains).
 * SCALE_ISO_ALL_MPK  — every component gets its own key (hits budget at N=15).
 * SCALE_ISO_PATH_MPK — only hub + target leaf; siblings stay route-only.
 */
#define SCALE_MPK_USABLE_KEYS	14
#define SCALE_ISO_ALL_MPK	0
#define SCALE_ISO_PATH_MPK	1

struct scale_star {
	Fabric *hub;
	Fabric *leaves[SCALE_MAX_LIBS - 1];
	unsigned n_libs;
	unsigned n_leaves;
	int iso_mode;
	unsigned keys_used;
	char target_svc[8];
};

int scale_star_build(struct scale_star *star, unsigned n_libs);
int scale_star_build_iso(struct scale_star *star, unsigned n_libs, int iso_mode);
void scale_star_destroy(struct scale_star *star);
const char *scale_star_target_svc(const struct scale_star *star);
uint32_t scale_star_leaf_id(const struct scale_star *star, unsigned leaf_idx);
unsigned scale_star_keys_used(const struct scale_star *star);

/*
 * Fat chain / grid:
 *   L hops → (L+1) layer hubs on the path
 *   each layer has K libraries (hub + K-1 siblings)
 * Bind next-hop BEFORE siblings so DFS takes the path first (O(L)),
 * not an O(L·K) sibling scan — matches good soft-bus scalability
 * (overhead stays flat as K grows).
 * Service registered only on the destination hub (layer L).
 * MPK isolation on path hubs only (≤ L+1 keys; siblings are route-only).
 */
#define SCALE_MAX_LAYERS	5	/* max hops */
#define SCALE_MAX_PER_LAYER	2000

struct scale_grid {
	unsigned n_hops;		/* L */
	unsigned libs_per_layer;	/* K */
	unsigned n_layers;		/* L + 1 */
	Fabric **nodes;			/* flat: layer-major, [layer*K + i] */
	char target_svc[8];
};

int scale_grid_build(struct scale_grid *g, unsigned n_hops,
		     unsigned libs_per_layer);
void scale_grid_destroy(struct scale_grid *g);

/* Path source = layer0 hub; target service name on layer-L hub. */
Fabric *scale_grid_src(const struct scale_grid *g);
const char *scale_grid_target_svc(const struct scale_grid *g);

/*
 * Service-map sizing helpers (for map-scale bench).
 * Recreate controller map with given bucket capacity, then fill entries.
 */
#define SCALE_MAX_MAP_ENTRIES	16384

int scale_map_recreate(Fabric *fabric, uint32_t capacity);
int scale_map_fill(Fabric *fabric, unsigned n_entries, unsigned target_idx,
		   char *out_name, unsigned out_len);

#endif /* SCALE_TOPO_H */
