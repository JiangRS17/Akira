#include "include/backends.h"
#include "../message/include/message.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SOFTWARE_MAX_ARGS 6

typedef struct {
    uint32_t bridge_id;
} SoftwareBridgeState;

static uint64_t software_eval(FabricMsg *m)
{
    uint64_t ret = 0;
    uint64_t args[SOFTWARE_MAX_ARGS] = {0};
    void *func;
    int argc;
    int index;

    if (!m || !m->func_ptr)
        return 0;

    argc = m->argc;
    if (argc < 0)
        argc = 0;
    if (argc > SOFTWARE_MAX_ARGS)
        argc = SOFTWARE_MAX_ARGS;

    for (index = 0; index < argc; index++)
        args[index] = (uint64_t)m->args[index];

    func = m->func_ptr;

    asm volatile (
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "movq %3, %%rdx\n"
        "movq %4, %%rcx\n"
        "movq %5, %%r8\n"
        "movq %6, %%r9\n"
        "call *%7\n"
        "movq %%rax, %0\n"
        : "=r"(ret)
        : "r"(args[0]), "r"(args[1]), "r"(args[2]), "r"(args[3]),
          "r"(args[4]), "r"(args[5]), "r"(func)
        : "rdi", "rsi", "rdx", "rcx", "r8", "r9", "rax", "memory"
    );

    return ret;
}

static int software_enter(FabricBridge *b, FabricMsg *m)
{
    uint64_t ret;

    if (!b || !m || !m->func_ptr || !b->iso_state)
        return -1;

    ret = software_eval(m);

    if (m->has_ret && m->ret_ptr && m->ret_size >= sizeof(uint64_t))
        memcpy(m->ret_ptr, &ret, sizeof(uint64_t));

    m->done = 1;
    return FABRIC_ISO_ENTER_DONE;
}

static int software_leave(FabricBridge *b, FabricMsg *m)
{
    (void)b;
    (void)m;
    return 0;
}

static void *software_init(FabricBridge *b, void *cfg)
{
    SoftwareBridgeState *st;

    (void)cfg;

    if (!b || !b->upstream_fabric || b->downstream_count == 0)
        return NULL;

    st = calloc(1, sizeof(*st));
    if (!st)
        return NULL;

    st->bridge_id = b->bridge_id;
    return st;
}

static void software_destroy(FabricBridge *b, void *state)
{
    (void)b;
    free(state);
}

FabricIsolationBackend sw_backend = {
    .name = "software-sync-backend",
    .enter = software_enter,
    .leave = software_leave,
    .init = software_init,
    .destroy = software_destroy
};
