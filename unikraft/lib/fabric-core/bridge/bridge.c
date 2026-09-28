#include "./include/bridge.h"
#include "../include/fabric_error.h"
#include "../backends/include/backends.h"
#include <flexos/isolation.h>


/* global brridge id counter */
static uint32_t g_bridge_id_counter = 0;
/* =========================================================
 * backend selector (registry version)
 * ========================================================= */

typedef struct {
    FabricIsolationType type;
    const FabricIsolationBackend *backend;
} FabricBackendRegistry;

/* =========================================================
 * selector
 * ========================================================= */
/* backend registry table */
static FabricBackendRegistry registry[] = {
    { FABRIC_ISO_MPK, &mpk_backend },
    { FABRIC_ISO_EPT, &ept_backend },
    { FABRIC_ISO_TRUSTZONE, &tz_backend },
    { FABRIC_ISO_SOFTWARE, &sw_backend }
};

static const size_t registry_size =
    sizeof(registry) / sizeof(registry[0]);


static const FabricIsolationBackend *
select_backend(FabricIsolationType type)
{
    for (size_t i = 0; i < registry_size; i++) {
        if (registry[i].type == type) {
            return registry[i].backend;
        }
    }
    return &sw_backend;
}

FabricBridge *fabric_bridge_create(Fabric *fabric) {
    // if(!owner) return NULL;

    FabricBridge *bridge = uk_zalloc(flexos_shared_alloc, sizeof(*bridge));
    if (!bridge) {
        return NULL;
    }

    bridge->bridge_id = __sync_fetch_and_add(&g_bridge_id_counter, 1);
    bridge->upstream_fabric = fabric;
    bridge->downstream_fabrics = NULL;
    bridge->downstream_count = 0;
    bridge->downstream_cap = 0;

    bridge->iso_backend = NULL; // 暂时不加
    bridge->iso_state = NULL;

    // bridge->cap_table.count = 0;

    return bridge;
}


void fabric_bridge_destroy(FabricBridge *bridge)
{
    if (!bridge)
        return;
    if (bridge->downstream_fabrics)
        uk_free(flexos_shared_alloc, bridge->downstream_fabrics);
    uk_free(flexos_shared_alloc, bridge);
}


int fabric_bridge_bind_upstream(FabricBridge *bridge, Fabric *fabric)
{
    if (!bridge || !fabric) return FABRIC_ERROR;
    if (bridge->upstream_fabric) return FABRIC_ERROR; // 只能绑定一次

    bridge->upstream_fabric = fabric;
    return FABRIC_SUCCESS;
}

 
int fabric_bridge_bind_downstream(FabricBridge *bridge, Fabric *fabric)
{
    Fabric **neu;
    size_t ncap;

    if (!bridge || !fabric)
        return FABRIC_ERROR;

    if (bridge->downstream_count >= FABRIC_MAX_DOWNSTREAM)
        return FABRIC_ERROR;

    if (bridge->downstream_count >= bridge->downstream_cap) {
        ncap = bridge->downstream_cap ? bridge->downstream_cap * 2 : 4;
        if (ncap > FABRIC_MAX_DOWNSTREAM)
            ncap = FABRIC_MAX_DOWNSTREAM;
        neu = uk_zalloc(flexos_shared_alloc, ncap * sizeof(Fabric *));
        if (!neu)
            return FABRIC_ERROR;
        if (bridge->downstream_fabrics && bridge->downstream_count) {
            size_t i;

            for (i = 0; i < bridge->downstream_count; i++)
                neu[i] = bridge->downstream_fabrics[i];
            uk_free(flexos_shared_alloc, bridge->downstream_fabrics);
        }
        bridge->downstream_fabrics = neu;
        bridge->downstream_cap = ncap;
    }

    bridge->downstream_fabrics[bridge->downstream_count++] = fabric;
    return FABRIC_SUCCESS;
}

Fabric **fabric_bridge_get_downstreams(FabricBridge *bridge, size_t *count)
{
    if (!bridge || !count) return NULL;
    *count = bridge->downstream_count;
    return bridge->downstream_fabrics;
}

Fabric *fabric_bridge_get_upstream(FabricBridge *bridge)
{
    if (!bridge) return NULL;
    return bridge->upstream_fabric;
}

Fabric *fabric_bridge_get_owner(FabricBridge *bridge)
{
    if (!bridge) return NULL;
    return bridge->upstream_fabric;
}


int fabric_bridge_set_isolation(FabricBridge *bridge, FabricIsolationType type, void *config) 
{
    if(!bridge) return FABRIC_ERROR;

    const FabricIsolationBackend *backend = select_backend(type);

    if (!backend) return FABRIC_ERROR; 

    // bind backend 
    bridge->iso_backend = backend; 

    //init backend state
    if(backend->init) 
        bridge->iso_state = backend->init(bridge, config); 

    return FABRIC_SUCCESS;

}


const FabricIsolationBackend *fabric_bridge_get_isolation(FabricBridge *bridge)
{
    if (!bridge) return NULL;
    return bridge->iso_backend;
}
