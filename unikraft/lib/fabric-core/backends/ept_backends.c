/* =========================================================================
 * ept_backends.c — FlexOS VM/EPT (VMEPT) isolation backend
 * =========================================================================
 *
 * Mirrors FlexOS vmept gates: one Fabric = one compartment ID.
 * Cross-fabric enter fills the shared RPC ctrl and waits for the peer VM
 * (same protocol as flexos_vmept_gate* in flexos-core).
 *
 * enter() return codes (see bridge.h):
 *   0  — ok, controller should run fabric_receive_message locally
 *   1  — ok, RPC already completed (skip local receive)
 *  -1  — error
 *
 * Requires CONFIG_LIBFLEXOS_VMEPT and kraft multi-image VMEPT build.
 * ========================================================================= */

#include "include/backends.h"
#include "../message/include/message.h"
#include <uk/config.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if CONFIG_LIBFLEXOS_VMEPT
#include <flexos/impl/vmept.h>
#include <uk/thread.h>
#include <uk/assert.h>
#endif

#define EPT_MAX_COMPS 16

/* fabric_id -> VMEPT compartment id; 0xff = unset */
static uint8_t g_fabric_comp_map[MAX_FABRIC];
static uint8_t g_next_comp_id;
static int g_ept_map_inited;

static void ept_map_init_once(void)
{
	if (g_ept_map_inited)
		return;
	memset(g_fabric_comp_map, 0xff, sizeof(g_fabric_comp_map));
	g_next_comp_id = 0;
	g_ept_map_inited = 1;
}

static uint8_t ept_alloc_comp(uint32_t fabric_id)
{
	ept_map_init_once();

	if (fabric_id >= MAX_FABRIC)
		return 0xff;

	if (g_fabric_comp_map[fabric_id] != 0xff)
		return g_fabric_comp_map[fabric_id];

	if (g_next_comp_id >= EPT_MAX_COMPS)
		return 0xff;

#if CONFIG_LIBFLEXOS_VMEPT
	if (g_next_comp_id >= FLEXOS_VMEPT_COMP_COUNT)
		return 0xff;
#endif

	g_fabric_comp_map[fabric_id] = g_next_comp_id++;
	return g_fabric_comp_map[fabric_id];
}

static int ept_lookup_comp(uint32_t fabric_id, uint8_t *comp_out)
{
	if (fabric_id >= MAX_FABRIC || !comp_out)
		return -1;
	if (!g_ept_map_inited || g_fabric_comp_map[fabric_id] == 0xff)
		return -1;
	*comp_out = g_fabric_comp_map[fabric_id];
	return 0;
}

#if CONFIG_LIBFLEXOS_VMEPT

static int ept_vmept_rpc(uint8_t key_from, uint8_t key_to, FabricMsg *m)
{
	volatile struct flexos_vmept_rpc_ctrl *ctrl;
	int argc;
	uint8_t returns_val;
	int i;

	if (!m || !m->func_ptr)
		return -EINVAL;

	ctrl = uk_thread_current()->ctrl;
	if (!ctrl)
		return -EINVAL;

	argc = m->argc;
	if (argc < 0)
		argc = 0;
	if (argc > FLEXOS_VMEPT_MAX_PARAMS)
		return -E2BIG;

	returns_val = m->has_ret ? 1 : 0;

	flexos_vmept_ctrl_set_state(ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN);
	flexos_vmept_ctrl_set_func(ctrl, m->func_ptr, (uint8_t)argc, returns_val);

	for (i = 0; i < argc; i++)
		ctrl->parameters[i] = (uint64_t)m->args[i];
	for (; i < FLEXOS_VMEPT_MAX_PARAMS; i++)
		ctrl->parameters[i] = 0;

	flexos_vmept_ctrl_inc_recursion(ctrl);
	flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED,
					     key_from, key_to);
	flexos_vmept_wait_for_rpc(ctrl);
	flexos_vmept_ctrl_set_state(ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN);

	if (m->has_ret && m->ret_ptr && m->ret_size > 0) {
		uint64_t ret = ctrl->ret;
		size_t n = m->ret_size < sizeof(ret) ? m->ret_size : sizeof(ret);

		memcpy(m->ret_ptr, &ret, n);
	}

	flexos_vmept_ctrl_dec_recursion(ctrl);
	flexos_vmept_ctrl_set_state(ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE);

	m->done = 1;
	return 0;
}

static void *ept_init(FabricBridge *b, void *cfg)
{
	EPTBridgeState *st;
	uint8_t comp_id;

	if (!b)
		return NULL;

	st = calloc(1, sizeof(*st));
	if (!st)
		return NULL;

	st->bridge_id = b->bridge_id;

	if (cfg) {
		EPTConfig *c = (EPTConfig *)cfg;

		comp_id = ept_alloc_comp(c->fabric_id);
		st->fabric_id = c->fabric_id;
	} else {
		comp_id = ept_alloc_comp(b->bridge_id);
		st->fabric_id = b->bridge_id;
	}

	if (comp_id == 0xff) {
		free(st);
		return NULL;
	}

	st->comp_id = comp_id;

	printf("[AKIRA-EPT] init bridge %u fabric=%u comp_id=%u "
	       "(local_comp=%u count=%u)\n",
	       b->bridge_id, st->fabric_id, st->comp_id,
	       (unsigned)flexos_vmept_comp_id,
	       (unsigned)FLEXOS_VMEPT_COMP_COUNT);

	return st;
}

static int ept_enter(FabricBridge *b, FabricMsg *m)
{
	EPTBridgeState *st;
	uint8_t src_comp, dst_comp;

	if (!b || !m || !b->iso_state)
		return -1;

	st = (EPTBridgeState *)b->iso_state;

	if (ept_lookup_comp(m->src_fabric_id, &src_comp) != 0)
		src_comp = st->comp_id;

	if (ept_lookup_comp(m->dst_fabric_id, &dst_comp) != 0)
		return -1;

	/*
	 * Local evaluation when:
	 *  - both fabrics map to the same VMEPT compartment, or
	 *  - the callee compartment is this VM (common during library-VM
	 *    boot: fabric_gate starts on fabric 0 but routes to local lwip).
	 * Otherwise cross-VM RPC from this compartment to dst.
	 */
	if (src_comp == dst_comp || dst_comp == flexos_vmept_comp_id)
		return FABRIC_ISO_ENTER_OK;

	if (ept_vmept_rpc(flexos_vmept_comp_id, dst_comp, m) != 0)
		return -1;

	return FABRIC_ISO_ENTER_DONE;
}

static int ept_leave(FabricBridge *b, FabricMsg *m)
{
	(void)b;
	(void)m;
	return 0;
}

static void ept_destroy(FabricBridge *b, void *state)
{
	(void)b;
	free(state);
}

FabricIsolationBackend ept_backend = {
	.name = "ept-vmept-flexos",
	.init = ept_init,
	.enter = ept_enter,
	.leave = ept_leave,
	.destroy = ept_destroy,
};

#else /* !CONFIG_LIBFLEXOS_VMEPT */

static void *ept_init(FabricBridge *b, void *cfg)
{
	(void)b;
	(void)cfg;
	printf("[AKIRA-EPT] CONFIG_LIBFLEXOS_VMEPT is not set; "
	       "EPT backend unavailable\n");
	return NULL;
}

static int ept_enter(FabricBridge *b, FabricMsg *m)
{
	(void)b;
	(void)m;
	return -1;
}

static int ept_leave(FabricBridge *b, FabricMsg *m)
{
	(void)b;
	(void)m;
	return 0;
}

static void ept_destroy(FabricBridge *b, void *state)
{
	(void)b;
	free(state);
}

FabricIsolationBackend ept_backend = {
	.name = "ept-vmept-flexos (disabled)",
	.init = ept_init,
	.enter = ept_enter,
	.leave = ept_leave,
	.destroy = ept_destroy,
};

#endif /* CONFIG_LIBFLEXOS_VMEPT */
