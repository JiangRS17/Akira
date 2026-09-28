#include "./include/controller.h"
#include "../bridge/include/bridge.h"
#include "../queue/include/queue.h"
#include "../component/include/component.h"
#include "../include/fabric_error.h"
#include "../message/include/message.h"
#include "../overhead/include/overhead.h"
#include "../fabric/include/fabric.h"

#include <uk/alloc.h>
#include <uk/print.h>
#include <flexos/isolation.h>
#include <stdio.h>
#include <string.h>

/* ==================== 全局控制器表 ==================== */

FabricController *g_fabric_controllers[FABRIC_CTRL_MAX] = {0};
uint32_t g_fabric_controller_count = 0;
static uint32_t g_fabric_controller_id_counter = 0;

void fabric_controller_global_reset(void)
{
	memset(g_fabric_controllers, 0, sizeof(g_fabric_controllers));
	g_fabric_controller_count = 0;
	g_fabric_controller_id_counter = 0;
}

/* ==================== 控制器生命周期 ==================== */

FabricController *fabric_controller_create(Fabric *fabric)
{
	uint32_t id;

	if (!fabric)
		return NULL;

	id = __sync_fetch_and_add(&g_fabric_controller_id_counter, 1);
	if (id >= FABRIC_CTRL_MAX)
		return NULL;

	FabricController *ctrl =
		uk_zalloc(flexos_shared_alloc, sizeof(*ctrl));
	if (!ctrl)
		return NULL;

	ctrl->controller_id  = id;
	ctrl->fabric = fabric;

	/* 创建并初始化仲裁器 */
	ctrl->arbiter = uk_zalloc(flexos_shared_alloc, sizeof(*ctrl->arbiter));
	if (!ctrl->arbiter) {
		uk_free(flexos_shared_alloc, ctrl);
		return NULL;
	}
	fabric_arbiter_init(ctrl->arbiter);

	ctrl->function_map = function_map_create(16);  /* smaller: large-N scale ok */
	if (!ctrl->function_map) {
		uk_free(flexos_shared_alloc, ctrl->arbiter);
		uk_free(flexos_shared_alloc, ctrl);
		return NULL;
	}

	/* 注册到全局控制器表 */
	g_fabric_controllers[id] = ctrl;
	g_fabric_controller_count++;

	return ctrl;
}

int fabric_controller_destroy(FabricController *ctrl)
{
	if (!ctrl)
		return FABRIC_ERROR;

	/* 从全局表移除 */
	if (ctrl->controller_id < FABRIC_CTRL_MAX)
		g_fabric_controllers[ctrl->controller_id] = NULL;

	if (ctrl->function_map)
		function_map_destroy(ctrl->function_map);

	if (ctrl->arbiter)
		uk_free(flexos_shared_alloc, ctrl->arbiter);

	/* 解除与 Fabric 的绑定 */
	if (ctrl->fabric)
		ctrl->fabric->controller = NULL;

	uk_free(flexos_shared_alloc, ctrl);

	if (g_fabric_controller_count > 0)
		g_fabric_controller_count--;

	return FABRIC_SUCCESS;
}

FabricController *fabric_controller_get(uint32_t id)
{
	if (id >= FABRIC_CTRL_MAX)
		return NULL;
	return g_fabric_controllers[id];
}

/* ==================== 消息队列操作 ==================== */
static FabricQueue *ctrl_get_ready_queue(FabricController *ctrl)
{
	Fabric *fab;

	if (!ctrl)
		return NULL;

	fab = ctrl->fabric;
	if (!fab || !fab->queues || fab->queues_count == 0)
		return NULL;

	return fab->queues[FABRIC_QUEUE_TYPE_READY];
}

int fabric_controller_enqueue(FabricController *ctrl, FabricMsg *msg)
{
	FabricQueue *q;
	int ret;

	if (!ctrl || !msg)
		return FABRIC_ERROR;

	q = ctrl_get_ready_queue(ctrl);  // 获取就绪态队列
	if (!q)
		return FABRIC_ERROR;

	ret = fabric_queue_enqueue(q, msg);

	return ret;
}

FabricMsg *fabric_controller_dequeue(FabricController *ctrl)
{
	FabricQueue *q;
	FabricMsg *msg;

	if (!ctrl)
		return NULL;

	q = ctrl_get_ready_queue(ctrl);  // 获取就绪态队列
	if (!q)
		return NULL;

	msg = fabric_queue_dequeue(q);

	return msg;
}

/* ==================== DFS 路由 ==================== */
// static FabricController *route_dfs_visit(FabricController *ctrl,
// 		uint64_t service_id,
// 		uint32_t *visited,
// 		uint32_t *vcnt)
// {
// 	uint32_t fid;
// 	uint32_t i;
// 	FabricBridge *bridge;
// 	FabricController *found;

// 	if (!ctrl || !ctrl->fabric || !ctrl->fabric->bridge)
// 		return NULL;

// 	fid = ctrl->fabric->fabric_id;

// 	/* 检查是否已经访问过此 Fabric，避免无限循环 */
// 	for (i = 0; i < *vcnt; i++) {
// 		if (visited[i] == fid)
// 			return NULL;
// 	}
// 	if (*vcnt >= FABRIC_MAX_VISITED)
// 		return NULL;

// 	visited[(*vcnt)++] = fid;

// 	/* 热路径仅按稳定 service_id 查找，避免字符串匹配和指针回退。 */
// 	if (ctrl->function_map &&
// 	    function_map_contains_service_id(ctrl->function_map, service_id)) {
// 		return ctrl;
// 	}

// 	/* 取出当前 Fabric 的 Bridge，遍历其连接的其他 Fabric */
// 	bridge = ctrl->fabric->bridge;
// 	if (!bridge)
// 		return NULL;

// 	/* 优先向下行 Fabric 搜索（深度优先） */
// 	for (i = 0; i < (uint32_t)bridge->downstream_count; i++) {
// 		Fabric *down = bridge->downstream_fabrics[i];

// 		if (!down || !down->controller)
// 			continue;

// 		found = route_dfs_visit(down->controller, service_id,
// 					visited, vcnt);
// 		if (found)
// 			return found;
// 	}

// 	/* 再尝试上行 Fabric */
// 	if (bridge->upstream_fabric && bridge->upstream_fabric->controller) {
// 		found = route_dfs_visit(bridge->upstream_fabric->controller,
// 					service_id, visited, vcnt);
// 		if (found)
// 			return found;
// 	}

// 	return NULL;
// }

// FabricController *fabric_controller_route_dfs(FabricController *ctrl,
// 					      FabricMsg *msg)
// {
// 	uint32_t visited[FABRIC_MAX_VISITED];
// 	uint32_t vcnt = 0;

// 	if (!ctrl || !msg || !msg->service_id)
// 		return NULL;
// 	return route_dfs_visit(ctrl, msg->service_id,
// 			       visited, &vcnt);
// }


static Fabric *execute_dfs(FabricController *ctrl,
			uint64_t service_id,
			uint8_t *visited) {
    if(!ctrl || !ctrl->fabric) {
        return NULL;
    }
    Fabric *fabric = ctrl->fabric;
    uint32_t fid = fabric->fabric_id;

    // 如果当前 fabric 已访问 退出
    if (visited[fid / 8] & (1u << (fid % 8)))
        return NULL;

    visited[fid / 8] |= (1u << (fid % 8));  // 标记当前总线已访问
    fabric_dfs_visit_add(1);

    // 首先检查当前Fabric的控制器是否提供目标服务
    if (ctrl->function_map &&
	    function_map_contains_service_id(ctrl->function_map, service_id)) {
		return fabric;
	}

    // 检查下行软总线
    FabricBridge *bridge = fabric->bridge;
    if (!bridge) {
        return NULL;
    }

    size_t ds_count = 0;
    Fabric **downstreams = fabric_bridge_get_downstreams(bridge, &ds_count);
    if (!downstreams || ds_count == 0) {
        return NULL;
    }

    /*
     * Forward-first DFS (bidi edges put both neighbors in downstream[]):
     * pass 0 — prefer higher fabric_id (chain “下一跳”方向)
     * pass 1 — then lower fabric_id (回边 / 上游)
     * Creation order in fabric_init assigns increasing ids along the chain,
     * so id > fid means forward without changing the bind API.
     */
    for (int pass = 0; pass < 2; pass++) {
	    for (size_t i = 0; i < ds_count; i++) {
		Fabric *sub_fabric = downstreams[i];
		uint32_t sid;

		if (!sub_fabric || !sub_fabric->controller)
			continue;

		sid = sub_fabric->fabric_id;
		if (pass == 0) {
			if (sid <= fid)
				continue;
		} else {
			if (sid >= fid)
				continue;
		}

		Fabric *found = execute_dfs(sub_fabric->controller, service_id,
					    visited);
		if (found)
			return found;
	    }
    }

	return NULL;
}


// 执行路由搜索目标总线
Fabric* fabric_controller_route_dfs(FabricController *ctrl,
 					    FabricMsg *msg) {
    if (!ctrl || !msg || !msg->service_id) return NULL;

    /*
     * visited was on-stack (MAX_FABRIC/8 = 2KiB). Deep soft-bus chains
     * (≈14 hops) overflow the unikernel stack; keep it static instead.
     * Single-threaded gate path — safe for current Akira use.
     */
    static uint8_t visited[MAX_FABRIC / 8];

    memset(visited, 0, sizeof(visited));
    return execute_dfs(ctrl, msg->service_id, visited);
}


/* ==================== 消息处理 ==================== */

// int fabric_controller_process(FabricController *ctrl)
// {
// 	FabricQueue *q;
// 	FabricMsg *msg;
// 	Fabric *fab;
// 	FabricComponent *comp;
// 	int ret;

// 	if (!ctrl || !ctrl->fabric)
// 		return FABRIC_ERROR;

// 	fab = ctrl->controlFabric;
// 	q = ctrl_get_ready_queue(ctrl);
// 	if (!q)
// 		return FABRIC_ERROR;

// 	/* 通过仲裁器请求总线访问权 */
// 	fabric_arbiter_wait_for_access(ctrl->arbiter, ctrl->controlFabric);

// 	/* 从队列中取出消息 */
// 	msg = fabric_queue_dequeue(q);
// 	if (!msg) {
// 		fabric_arbiter_release_access(ctrl->arbiter);
// 		return FABRIC_ERROR;
// 	}

// 	/*
// 	 * 派发路径经 component_receive；实际查表使用 Controller::functionMap，
// 	 * 不在 Component 上持有 function_map。
// 	 */
// 	comp = fab->component;
// 	if (!comp)
// 		ret = FABRIC_ERROR;
// 	else
// 		ret = fabric_receive_message(comp, msg);

// 	/* 释放总线访问权 */
// 	fabric_arbiter_release_access(ctrl->arbiter);

// 	return ret;
// }
// int fabric_controller_process(FabricController *ctrl)
// {
// 	FabricQueue *q;
// 	FabricMsg *msg;
// 	Fabric *fab;
// 	int ret;

// 	if (!ctrl || !ctrl->fabric)
// 		return FABRIC_ERROR;

// 	fab = ctrl->fabric;
// 	q = ctrl_get_ready_queue(ctrl);
// 	if (!q)
// 		return FABRIC_ERROR;

// 	fabric_arbiter_wait_for_access(ctrl->arbiter, ctrl->fabric);

// 	if (!fabric_queue_is_empty(q)) {
// 		/* 从队列中取出一条消息并派发 */
// 		msg = fabric_queue_dequeue(q);
// 		if (!msg) {
// 			fabric_arbiter_release_access(ctrl->arbiter);
// 			return FABRIC_ERROR;
// 		}

// 		ret = fabric_receive_message(fab, msg);
// 		fabric_arbiter_release_access(ctrl->arbiter);
// 		return (ret == FABRIC_SUCCESS) ? FABRIC_SUCCESS : FABRIC_ERROR;
// 	}

// 	fabric_arbiter_release_access(ctrl->arbiter);
// 	return FABRIC_SUCCESS;
// }


// 消息处理入口
int fabric_controller_handle_request(FabricController *ctrl, FabricMsg *msg)
{
	Fabric *target_fabric;
	FabricController *target_controller;
	FabricBridge *src_bridge;
	struct fabric_overhead_stats *st;
	uint32_t abl;
	uint64_t t_ctrl0 = 0, t0 = 0, t1;
	int entered = 0;
	int is_cross_fabric;
	int ret;
	int prof;

	if (!ctrl || !msg)
		return FABRIC_ERROR;

	st = fabric_overhead_stats_ptr();
	abl = fabric_ablation_get();
	prof = fabric_overhead_enabled();
	if (prof)
		t_ctrl0 = fabric_ov_tsc();

	/* ---- route: DFS, or honor preset dst (domain-RR benches) ---- */
	if (prof)
		t0 = fabric_ov_tsc();
	if (msg->dst_fabric_id != (uint32_t)-1 &&
	    msg->dst_fabric_id < MAX_FABRIC &&
	    g_fabrics[msg->dst_fabric_id]) {
		target_fabric = g_fabrics[msg->dst_fabric_id];
	} else {
		target_fabric = fabric_controller_route_dfs(ctrl, msg);
		if (!target_fabric)
			return FABRIC_ERROR;
		msg->dst_fabric_id = target_fabric->fabric_id;
	}
	target_controller = target_fabric->controller;
	if (!target_controller)
		return FABRIC_ERROR;
	if (prof) {
		t1 = fabric_ov_tsc();
		st->route += t1 - t0;
	}

	/*
	 * @TODO: 目前队列看着没啥用，所以不经过队列转发
	 * 直接获取到目标总线然后执行服务就行
	 */

	src_bridge = ctrl->fabric->bridge;
	is_cross_fabric = (msg->src_fabric_id != msg->dst_fabric_id);

	if (prof)
		t0 = fabric_ov_tsc();
	if (!(abl & FABRIC_ABL_SKIP_ISO) &&
	    is_cross_fabric && src_bridge && src_bridge->iso_backend &&
	    src_bridge->iso_backend->enter) {
		int enter_rc = src_bridge->iso_backend->enter(src_bridge, msg);

		if (enter_rc < 0) {
			uk_pr_err("[fabric-ctrl] isolation enter rejected\n");
			return FABRIC_ERROR;
		}
		entered = 1;
		/* EPT/VMEPT (and similar): enter already dispatched via RPC */
		if (enter_rc == FABRIC_ISO_ENTER_DONE) {
			if (prof) {
				t1 = fabric_ov_tsc();
				st->iso_enter += t1 - t0;
			}
			if (prof)
				t0 = fabric_ov_tsc();
			if (src_bridge->iso_backend->leave)
				src_bridge->iso_backend->leave(src_bridge, msg);
			if (prof) {
				t1 = fabric_ov_tsc();
				st->iso_leave += t1 - t0;
				st->total_ctrl += t1 - t_ctrl0;
				st->samples++;
			}
			return FABRIC_SUCCESS;
		}
	}
	if (prof) {
		t1 = fabric_ov_tsc();
		st->iso_enter += t1 - t0;
	}

	if (prof)
		t0 = fabric_ov_tsc();
	if (!(abl & FABRIC_ABL_SKIP_ARBITER))
		fabric_arbiter_wait_for_access(target_controller->arbiter,
					       target_fabric);
	if (prof) {
		t1 = fabric_ov_tsc();
		st->arbiter += t1 - t0;
	}

	ret = fabric_receive_message(target_fabric, msg);

	if (prof)
		t0 = fabric_ov_tsc();
	if (!(abl & FABRIC_ABL_SKIP_ARBITER))
		fabric_arbiter_release_access(target_controller->arbiter);
	if (prof) {
		t1 = fabric_ov_tsc();
		st->arbiter += t1 - t0;
	}

	if (prof)
		t0 = fabric_ov_tsc();
	if (entered && src_bridge->iso_backend->leave)
		src_bridge->iso_backend->leave(src_bridge, msg);
	if (prof) {
		t1 = fabric_ov_tsc();
		st->iso_leave += t1 - t0;
		st->total_ctrl += t1 - t_ctrl0;
		st->samples++;
	}

	if (ret != FABRIC_SUCCESS)
		return FABRIC_ERROR;

	return FABRIC_SUCCESS;
}