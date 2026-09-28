#ifndef FABRIC_BACKENDS_H
#define FABRIC_BACKENDS_H

#include "../../bridge/include/bridge.h"

/*
 * MPK backend configuration
 * Passed as cfg parameter to fabric_bridge_set_isolation()
 * when type == FABRIC_ISO_MPK
 */
/* [旧设计] 每个 bridge 独立分配 upstream/downstream key */
// typedef struct {
//     uint32_t upstream_domain;  /* 上游保护域编号 */
//     uint32_t isolation_level;  /* 隔离级别（预留） */
// } MPKConfig;

/* [新设计] 全局 Key 分配器：一个 fabric = 一个 MPK key */
typedef struct {
    uint32_t fabric_id;        /* fabric 的 ID，用于全局 key 分配 */
} MPKConfig;

/* MPK bridge 运行时状态（供外部访问 key 信息） */
typedef struct {
    uint32_t bridge_id;
    uint32_t my_key;      /* 本 fabric 的 MPK key */
} MPKBridgeState;

/*
 * EPT / FlexOS VMEPT: one fabric = one compartment id.
 * Passed as cfg to fabric_bridge_set_isolation(..., FABRIC_ISO_EPT, ...).
 */
typedef struct {
    uint32_t fabric_id;
} EPTConfig;

typedef struct {
    uint32_t bridge_id;
    uint32_t fabric_id;
    uint8_t  comp_id;     /* VMEPT compartment id (key_from / key_to) */
} EPTBridgeState;

/* Built-in backends */
extern FabricIsolationBackend mpk_backend;
extern FabricIsolationBackend ept_backend;
extern FabricIsolationBackend tz_backend;
extern FabricIsolationBackend sw_backend;

/* Reset fabric→key map (call with fabric_global_reset when rebuilding topos). */
void akira_mpk_key_reset(void);

/*
 * Fine-grain path kernels for C_sw / C_mech microbench (Part C).
 * Operate on an already-initialised MPK bridge + cross-fabric msg.
 * Returns 0 on success, -1 on error / MPK unavailable.
 */
int akira_mpk_path_sw_only(FabricBridge *b, struct FabricMsg *m);
int akira_mpk_path_mech_only(FabricBridge *b, struct FabricMsg *m);
int akira_mpk_path_full(FabricBridge *b, struct FabricMsg *m);

#endif /* FABRIC_BACKENDS_H */