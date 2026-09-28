#include "include/backends.h"



// fabric_bridge_enforce()
//         ↓
//    enter()
//         ↓
//    (routing / forwarding)
//         ↓
//    leave()

static int no_enter(FabricBridge *b, struct FabricMsg *m)
{
    (void)b; (void)m;
    return 0;
}

static int no_leave(FabricBridge *b, struct FabricMsg *m)
{
    (void)b; (void)m;
    return 0;
}


/* ================= TrustZone ================= */
static void *tz_init(FabricBridge *b, void *cfg)
{
    (void)b; (void)cfg;
    return NULL;
}

static void tz_destroy(FabricBridge *b, void *state)
{
    (void)b; (void)state;
}

/* =========================================================
 * backend registry
 * ========================================================= */

FabricIsolationBackend tz_backend = {
    .name = "trustzone",
    .enter = no_enter,
    .leave = no_leave,
    .init = tz_init,
    .destroy = tz_destroy
};

/* sw_backend 已移至 software_backends.c（包含完整实现），此处不再重复定义 */
// FabricIsolationBackend sw_backend = {
//     .name = "software",
//     .enter = no_enter,
//     .leave = no_leave,
//     .init = NULL,
//     .destroy = NULL,
// };
