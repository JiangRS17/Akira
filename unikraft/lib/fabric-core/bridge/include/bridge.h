#ifndef FABRIC_BRIDGE_H
#define FABRIC_BRIDGE_H

#include "../../fabric/include/fabric.h"

/* Cap on fan-out per bridge (dynamic array grows up to this). */
#define FABRIC_MAX_DOWNSTREAM 4096
#define FABRIC_MAX_CAPS       17  // 每个桥最大 capability 条数

struct FabricMsg; 
struct FabricBridge; 

// 隔离机制类型
typedef enum {
    FABRIC_ISO_NONE = 0,
    FABRIC_ISO_MPK,
    FABRIC_ISO_EPT,
    FABRIC_ISO_TRUSTZONE,
    FABRIC_ISO_SOFTWARE
} FabricIsolationType; 


/*
 * enter() return codes:
 *   FABRIC_ISO_ENTER_OK   (0)  — proceed with local fabric_receive_message
 *   FABRIC_ISO_ENTER_DONE (1)  — backend already dispatched (e.g. VMEPT RPC)
 *   negative                   — error / rejected
 */
#define FABRIC_ISO_ENTER_OK    0
#define FABRIC_ISO_ENTER_DONE  1

typedef int (*fabric_isolation_enter_fn)(
    struct FabricBridge *bridge,
    struct FabricMsg *msg
);

typedef int (*fabric_isolation_leave_fn)(
    struct FabricBridge *bridge,
    struct FabricMsg *msg
);

typedef struct FabricIsolationBackend {
    const char *name;

    fabric_isolation_enter_fn enter;
    fabric_isolation_leave_fn leave;

    void *(*init)(struct FabricBridge *bridge, void *config);
    void (*destroy)(struct FabricBridge *bridge, void *state); 

} FabricIsolationBackend;


typedef struct FabricBridge{
    uint32_t bridge_id;

    /* ===== Topology ===== */
    Fabric *upstream_fabric;
    Fabric **downstream_fabrics; /* growable; NULL if empty */
    size_t downstream_count;
    size_t downstream_cap;

    /* ===== Isolation backend ===== */
    const FabricIsolationBackend *iso_backend;
    void *iso_state;

} FabricBridge;


/* =========================================================
 * lifecycle
 * ========================================================= */

FabricBridge *fabric_bridge_create(Fabric *fabric);

void fabric_bridge_destroy(FabricBridge *bridge);


/* =========================================================
 * topology management
 * ========================================================= */

/* 设置 upstream（只能设置一次） */
int fabric_bridge_bind_upstream(FabricBridge *bridge, Fabric *fabric);

int fabric_bridge_bind_downstream(FabricBridge *bridge, Fabric *fabric);

/* 添加 downstream */
int fabric_bridge_add_downstream(FabricBridge *bridge, Fabric *fabric);

/* 获取 downstream（controller 用） */
Fabric **fabric_bridge_get_downstreams(FabricBridge *bridge, size_t *count);

/* 获取 upstream */
Fabric *fabric_bridge_get_upstream(FabricBridge *bridge);

Fabric *fabric_bridge_get_owner(FabricBridge *bridge);


/* =========================================================
 * isolation policy (UNIFIED ENTRY POINT)
 * ========================================================= */
int fabric_bridge_set_isolation(FabricBridge *bridge, FabricIsolationType type, void *config); 

const FabricIsolationBackend *fabric_bridge_get_isolation(FabricBridge *bridge);

#endif /* FABRIC_BRIDGE_H */