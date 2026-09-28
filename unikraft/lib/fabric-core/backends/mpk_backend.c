/* =========================================================================
 * mpk_backend.c — 基于 Intel MPK (Memory Protection Keys) 的隔离后端
 * =========================================================================
 *
 * 设计原则：一个 Fabric = 一个 MPK Key
 *
 *   - 全局 Key 分配器：fabric_id → MPK key（1~15），key 0 保留给共享内存
 *   - enter：临时将目标 fabric 的 key 设为 RW（只改 2 位）
 *   - leave：将目标 fabric 的 key 恢复为 NONE
 *   - 不使用 wrpkru 覆盖整个 PKRU，只逐 key 操作
 *
 * ========================================================================= */

#include "include/backends.h"
#include "../message/include/message.h"
#include <overhead.h>
#include <uk/config.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if CONFIG_LIBFLEXOS_INTELPKU
#include <flexos/impl/intelpku.h>
#endif

#if CONFIG_LIBFLEXOS_INTELPKU

/* =========================================================================
 * 全局 Key 分配器
 * ========================================================================= */

#define MAX_PKEYS 16

/* 权限枚举 */
typedef enum {
    AKIRA_PERM_RW,     /* 读写 */
    AKIRA_PERM_RO,     /* 只读（当前未使用） */
    AKIRA_PERM_NONE    /* 禁止访问 */
} AkiraPerm;

/* fabric_id -> MPK key 映射表
 * key 0: 共享内存（硬件约定）
 * key 1: FlexOS 默认隔间（comp1）；Akira 不得占用，否则 init 时
 *        把 key1 设为 NONE 会 #PF on .bss/.data
 */
static uint32_t g_fabric_key_map[MAX_FABRIC] = {0};

/* 下一个可分配的 key（从 2 开始） */
static uint32_t g_next_mpk_key = 2;

void akira_mpk_key_reset(void)
{
	memset(g_fabric_key_map, 0, sizeof(g_fabric_key_map));
	g_next_mpk_key = 2;
}

/*
 * mpk_alloc_key — 为 fabric 分配一个 MPK key
 *
 * 如果该 fabric 已分配过 key，直接返回缓存值。
 * 否则分配下一个可用 key。
 */
static uint32_t mpk_alloc_key(uint32_t fabric_id)
{
    if (fabric_id >= MAX_FABRIC)
        return 0;

    if (g_fabric_key_map[fabric_id] != 0)
        return g_fabric_key_map[fabric_id];

    if (g_next_mpk_key >= MAX_PKEYS)
        return 0;  /* 所有 key 已用完 */

    uint32_t key = g_next_mpk_key++;
    g_fabric_key_map[fabric_id] = key;
    return key;
}

/*
 * mpk_lookup_key — 查询 fabric 的 MPK key
 */
static int mpk_lookup_key(uint32_t fabric_id, uint32_t *key_out)
{
    if (fabric_id >= MAX_FABRIC || !key_out)
        return -1;

    uint32_t key = g_fabric_key_map[fabric_id];
    if (key == 0)
        return -1;

    *key_out = key;
    return 0;
}


/* =========================================================================
 * PKRU 位操作工具
 * ========================================================================= */

static int pkru_set_rw(uint8_t key, uint32_t *val)
{
    if (key > 15)
        return -EINVAL;
    *val &= ~(1UL << (key * 2));
    *val &= ~(1UL << ((key * 2) + 1));
    return 0;
}

static int pkru_set_ro(uint8_t key, uint32_t *val)
{
    if (key > 15)
        return -EINVAL;
    *val &= ~(1UL << (key * 2));
    *val |= 1UL << ((key * 2) + 1);
    return 0;
}

static int pkru_set_no_access(uint8_t key, uint32_t *val)
{
    if (key > 15)
        return -EINVAL;
    *val |= 1UL << (key * 2);
    *val |= 1UL << ((key * 2) + 1);
    return 0;
}

int akira_mpk_set_perm(uint8_t key, AkiraPerm perm)
{
    uint32_t pkru = rdpkru();

    switch (perm) {
    case AKIRA_PERM_RW:
        pkru_set_rw(key, &pkru);
        break;
    
    case AKIRA_PERM_RO:
        pkru_set_ro(key, &pkru);
        break;

    case AKIRA_PERM_NONE:
        pkru_set_no_access(key, &pkru);
        break;
        
    default:
        return -1;
    }

    wrpkru(pkru);
    return 0;
}


/* =========================================================================
 * FabricIsolationBackend 接口实现
 * ========================================================================= */

static void *mpk_init(FabricBridge *b, void *cfg)
{
    MPKBridgeState *st = calloc(1, sizeof(MPKBridgeState));
    if (!st)
        return NULL;

    st->bridge_id = b->bridge_id;

    if (cfg) {
        MPKConfig *c = (MPKConfig *)cfg;
        st->my_key = mpk_alloc_key(c->fabric_id);
    } else {
        st->my_key = mpk_alloc_key(b->bridge_id);
    }

    if (st->my_key == 0) {
        free(st);
        return NULL;
    }

    akira_mpk_set_perm(st->my_key, AKIRA_PERM_RW);

    /* 禁止访问其他 fabric 的内存 */
    for (uint32_t i = 0; i < MAX_FABRIC; i++) {
        if (g_fabric_key_map[i] != 0 && g_fabric_key_map[i] != st->my_key)
            akira_mpk_set_perm(g_fabric_key_map[i], AKIRA_PERM_NONE);
    }

    /* FlexOS 默认隔间占用 key 1；Akira 不得关掉，否则 .bss/.data 会 PF_PK */
    akira_mpk_set_perm(1, AKIRA_PERM_RW);

    printf("[AKIRA-MPK] init bridge %u fabric_key=%u\n",
           b->bridge_id, st->my_key);

    return st;
}


/*
 * mpk_check — MPK 机制层校验（非拓扑策略）
 * 拓扑可达性由 controller route_dfs 负责。
 */
static int mpk_check(struct FabricBridge *b, struct FabricMsg *msg)
{
    MPKBridgeState *st = (MPKBridgeState *)b->iso_state;
    uint32_t dst_key;

    if (!st || !msg)
        return -1;

    if (st->my_key == 0 || st->my_key >= MAX_PKEYS)
        return -1;

    if (mpk_lookup_key(msg->dst_fabric_id, &dst_key) != 0)
        return -1;

    /* key 0 保留给共享内存，不可作为跨域目标 */
    if (dst_key == 0)
        return -1;

    return 0;
}


/*
 * Mechanism-only path: direct key map + rdpkru/wrpkru.
 * Skips mpk_check / validated lookup (bridge software path).
 * Used by FABRIC_ABL_ISO_MECH_ONLY to peel C_mech vs C_sw.
 */
static int mpk_enter_mech(FabricBridge *b, struct FabricMsg *m)
{
    uint32_t dst_key;
    uint32_t src_key;
    uint32_t pkru;

    (void)b;
    if (!m || m->dst_fabric_id >= MAX_FABRIC)
        return -1;

    dst_key = g_fabric_key_map[m->dst_fabric_id];
    if (dst_key == 0)
        return -1;

    pkru = rdpkru();
    if (m->src_fabric_id < MAX_FABRIC) {
        src_key = g_fabric_key_map[m->src_fabric_id];
        if (src_key != 0)
            pkru_set_rw((uint8_t)src_key, &pkru);
    }
    pkru_set_rw((uint8_t)dst_key, &pkru);
    wrpkru(pkru);
    return 0;
}

static int mpk_leave_mech(FabricBridge *b, struct FabricMsg *m)
{
    uint32_t dst_key;

    (void)b;
    if (!m || m->dst_fabric_id >= MAX_FABRIC)
        return -1;

    dst_key = g_fabric_key_map[m->dst_fabric_id];
    if (dst_key == 0)
        return -1;

    /* Same mechanism write as akira_mpk_set_perm(NONE), no software wrapper. */
    {
        uint32_t pkru = rdpkru();

        pkru_set_no_access((uint8_t)dst_key, &pkru);
        wrpkru(pkru);
    }
    return 0;
}

static int mpk_enter(FabricBridge *b, struct FabricMsg *m)
{
    MPKBridgeState *st = (MPKBridgeState *)b->iso_state;
    uint32_t dst_key;
    uint32_t src_key;

    if (!st || !m)
        return -1;

    /* Ablation: mechanism switch only (no check/lookup software). */
    if (fabric_ablation_get() & FABRIC_ABL_ISO_MECH_ONLY)
        return mpk_enter_mech(b, m);

    if (mpk_check(b, m) != 0)
        return -1;

    if (mpk_lookup_key(m->dst_fabric_id, &dst_key) != 0)
        return -1;

    /* 一次性修改 PKRU：恢复调用方 key + 开放目标 key（1 次 rdpkru + 1 次 wrpkru） */
    uint32_t pkru = rdpkru();

    if (mpk_lookup_key(m->src_fabric_id, &src_key) == 0)
        pkru_set_rw(src_key, &pkru);

    pkru_set_rw(dst_key, &pkru);

    wrpkru(pkru);

    return 0;
}


static int mpk_leave(FabricBridge *b, struct FabricMsg *m)
{
    MPKBridgeState *st = (MPKBridgeState *)b->iso_state;
    uint32_t dst_key;

    if (!st || !m)
        return -1;

    if (fabric_ablation_get() & FABRIC_ABL_ISO_MECH_ONLY)
        return mpk_leave_mech(b, m);

    if (mpk_lookup_key(m->dst_fabric_id, &dst_key) != 0)
        return -1;

    akira_mpk_set_perm(dst_key, AKIRA_PERM_NONE);

    // printf("[AKIRA-MPK] bridge %u LEAVE dst=%u key=%u\n",
    //        b->bridge_id, m->dst_fabric_id, dst_key);

    return 0;
}


static void mpk_destroy(FabricBridge *b, void *state)
{
    (void)b;
    free(state);
}

/*
 * Fine-grain kernels: software vs mechanism (no ablation flag dependency).
 * sw_only  — check + lookups for enter/leave, no PKRU write
 * mech_only — rdpkru/wrpkru only
 * full     — complete enter + leave (always full path)
 */
static volatile uint32_t g_mpk_bench_sink;

__attribute__((noinline))
int akira_mpk_path_sw_only(FabricBridge *b, struct FabricMsg *m)
{
	uint32_t dst_key = 0;
	uint32_t src_key = 0;

	if (!b || !m)
		return -1;

	/* Enter software path (same work as mpk_enter before wrpkru). */
	if (mpk_check(b, m) != 0)
		return -1;
	if (mpk_lookup_key(m->dst_fabric_id, &dst_key) != 0)
		return -1;
	(void)mpk_lookup_key(m->src_fabric_id, &src_key);

	/* Leave software path (lookup only; skip set_perm / wrpkru). */
	if (mpk_lookup_key(m->dst_fabric_id, &dst_key) != 0)
		return -1;

	/* Prevent DCE of lookups under -O2. */
	g_mpk_bench_sink = dst_key ^ src_key;
	return 0;
}

__attribute__((noinline))
int akira_mpk_path_mech_only(FabricBridge *b, struct FabricMsg *m)
{
	if (mpk_enter_mech(b, m) != 0)
		return -1;
	return mpk_leave_mech(b, m);
}

__attribute__((noinline))
int akira_mpk_path_full(FabricBridge *b, struct FabricMsg *m)
{
	MPKBridgeState *st;
	uint32_t dst_key;
	uint32_t src_key;
	uint32_t pkru;

	st = b ? (MPKBridgeState *)b->iso_state : NULL;
	if (!st || !m)
		return -1;

	/* Full enter (ignore ablation flags). */
	if (mpk_check(b, m) != 0)
		return -1;
	if (mpk_lookup_key(m->dst_fabric_id, &dst_key) != 0)
		return -1;

	pkru = rdpkru();
	if (mpk_lookup_key(m->src_fabric_id, &src_key) == 0)
		pkru_set_rw((uint8_t)src_key, &pkru);
	pkru_set_rw((uint8_t)dst_key, &pkru);
	wrpkru(pkru);

	/* Full leave. */
	if (mpk_lookup_key(m->dst_fabric_id, &dst_key) != 0)
		return -1;
	akira_mpk_set_perm((uint8_t)dst_key, AKIRA_PERM_NONE);

	return 0;
}


/* =========================================================================
 * 后端导出
 * ========================================================================= */
FabricIsolationBackend mpk_backend = {
    .name    = "mpk-flexos-aligned",
    .enter   = mpk_enter,
    .leave   = mpk_leave,
    .init    = mpk_init,
    .destroy = mpk_destroy,
};

#else /* !CONFIG_LIBFLEXOS_INTELPKU */

void akira_mpk_key_reset(void)
{
}

int akira_mpk_path_sw_only(FabricBridge *b, struct FabricMsg *m)
{
	(void)b;
	(void)m;
	return -1;
}

int akira_mpk_path_mech_only(FabricBridge *b, struct FabricMsg *m)
{
	(void)b;
	(void)m;
	return -1;
}

int akira_mpk_path_full(FabricBridge *b, struct FabricMsg *m)
{
	(void)b;
	(void)m;
	return -1;
}

static void *mpk_init(FabricBridge *b, void *cfg)
{
    (void)b;
    (void)cfg;
    printf("[AKIRA-MPK] CONFIG_LIBFLEXOS_INTELPKU is not set; "
           "MPK backend unavailable\n");
    return NULL;
}

static int mpk_enter(FabricBridge *b, struct FabricMsg *m)
{
    (void)b;
    (void)m;
    return -1;
}

static int mpk_leave(FabricBridge *b, struct FabricMsg *m)
{
    (void)b;
    (void)m;
    return 0;
}

static void mpk_destroy(FabricBridge *b, void *state)
{
    (void)b;
    free(state);
}

FabricIsolationBackend mpk_backend = {
    .name    = "mpk-flexos-aligned (disabled)",
    .enter   = mpk_enter,
    .leave   = mpk_leave,
    .init    = mpk_init,
    .destroy = mpk_destroy,
};

#endif /* CONFIG_LIBFLEXOS_INTELPKU */
