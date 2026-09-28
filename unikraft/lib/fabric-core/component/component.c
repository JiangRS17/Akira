#include "./include/component.h"
#include "../controller/include/controller.h"
#include "../fabric/include/fabric.h"
#include "../map/include/map.h"
#include "../include/fabric_error.h"
#include "../message/include/message.h"
#include "../include/fabric/isolation.h"
#include "../overhead/include/overhead.h"

#include <uk/alloc.h>
#include <string.h>
#include <flexos/isolation.h>
#include <stdio.h>

// component 全局计数器
static uint32_t g_component_id_counter = 0;

// 有返回值的函数调用
static long component_call_with_ret(func_t fn, FabricMsg *msg)
{
	return ((fabric_fn_ret_t)fn)(msg->args[0], msg->args[1], msg->args[2], msg->args[3],
				     msg->args[4], msg->args[5], msg->args[6], msg->args[7],
				     msg->args[8], msg->args[9], msg->args[10]);
}

// 无返回值的函数调用
static void component_call_no_ret(func_t fn, FabricMsg *msg)
{
	((fabric_fn_void_t)fn)(msg->args[0], msg->args[1], msg->args[2], msg->args[3],
			       msg->args[4], msg->args[5], msg->args[6], msg->args[7],
			       msg->args[8], msg->args[9], msg->args[10]);
}

// 创建组件并绑定到指定软总线
FabricComponent *fabric_component_create(Fabric *fabric)
{
	if (!fabric)
		return NULL;

	FabricComponent *comp =
		uk_zalloc(flexos_shared_alloc, sizeof(*comp));
	if (!comp)
		return NULL;

	comp->id = __sync_fetch_and_add(&g_component_id_counter, 1);
	comp->capabilities = 0;  // @TODO: 暂时不知道是什么
	comp->status = FB_COMP_STATE_INIT;  // 状态设置
	// comp->iso_domain = 0;
	comp->fabric = fabric;
	comp->controller = fabric->controller;
	// comp->business = NULL;
	comp->status = FB_COMP_STATE_REGISTERED;

	return comp;
}

int fabric_component_destroy(FabricComponent *comp)
{
	if (!comp)
		return FABRIC_ERROR;

	if (comp->status == FB_COMP_STATE_RUNNING ||
	    comp->status == FB_COMP_STATE_REGISTERED ||
	    comp->status == FB_COMP_STATE_ISOLATED)
		fabric_component_unregister(comp);

	uk_free(flexos_shared_alloc, comp);
	return FABRIC_SUCCESS;
}

// int component_register(FabricComponent *comp, Fabric *fabric)
// {
// 	if (!comp || !fabric)
// 		return FABRIC_ERROR;

// 	if (comp->status != FB_COMP_STATE_INIT)
// 		return FABRIC_ERROR;

// 	comp->status = FB_COMP_STATE_REGISTERED;
// 	comp->fabric = fabric;
// 	comp->controller = fabric->controller;

// 	return FABRIC_SUCCESS;
// }

int fabric_component_unregister(FabricComponent *comp)
{
	if (!comp)
		return FABRIC_ERROR;

	if (comp->status != FB_COMP_STATE_REGISTERED &&
	    comp->status != FB_COMP_STATE_RUNNING &&
	    comp->status != FB_COMP_STATE_ISOLATED)
		return FABRIC_ERROR;

	comp->fabric = NULL;
	comp->controller = NULL;
	comp->status = FB_COMP_STATE_UNREGISTERED;

	return FABRIC_SUCCESS;
}

/*
 * 仅应由 Controller 在出队后调用。
 * 函数表仅存于 Controller::functionMap；此处通过 comp->controller 访问并派发。
 */
int fabric_component_receive(FabricComponent *comp, FabricMsg *msg)
{
	FabricController *ctrl;
	func_t fn;
	struct fabric_overhead_stats *st;
	uint64_t t0 = 0, t1;
	uint32_t abl;
	int prof;

	if (!comp || !msg) {
		if (msg)
			msg->done = 1;
		return FABRIC_ERROR;
	}

	if (comp->status != FB_COMP_STATE_RUNNING &&
	    comp->status != FB_COMP_STATE_ISOLATED) {
		msg->done = 1;
		return FABRIC_ERROR;
	}

	ctrl = comp->controller;
	if (!ctrl || !ctrl->function_map) {
		msg->done = 1;
		return FABRIC_ERROR;
	}

	st = fabric_overhead_stats_ptr();
	abl = fabric_ablation_get();
	prof = fabric_overhead_enabled();

	if (prof)
		t0 = fabric_ov_tsc();
	if (abl & FABRIC_ABL_DIRECT_FN) {
		fn = (func_t)msg->func_ptr;
	} else {
		fn = function_map_get_by_service_id(ctrl->function_map,
						   msg->service_id);
	}
	if (prof) {
		t1 = fabric_ov_tsc();
		st->map_lookup += t1 - t0;
	}

	if (!fn) {
		msg->done = 1;
		return FABRIC_ERROR;
	}

	if (prof)
		t0 = fabric_ov_tsc();
	if (msg->has_ret) {
		long r = component_call_with_ret(fn, msg);

		if (msg->ret_ptr && msg->ret_size > 0) {
			size_t n = msg->ret_size < sizeof(r) ? msg->ret_size : sizeof(r);
			memcpy(msg->ret_ptr, &r, n);
		}
	} else {
		component_call_no_ret(fn, msg);
	}
	if (prof) {
		t1 = fabric_ov_tsc();
		st->callee += t1 - t0;
	}

	msg->done = 1;
	return FABRIC_SUCCESS;
}

// int component_switch_isolation(FabricComponent *comp, fb_iso_domain_t new_iso_domain)
// {
// 	if (!comp)
// 		return FABRIC_ERROR;

// 	comp->iso_domain = new_iso_domain;
// 	comp->status = FB_COMP_STATE_ISOLATED;

// 	return FABRIC_SUCCESS;
// }