#ifndef FABRIC_CONTROLLER_H
#define FABRIC_CONTROLLER_H

#include <stdint.h>
#include <stddef.h>

#include "../../fabric/include/fabric.h"
#include "../../arbiter/include/arbiter.h"
#include "../../map/include/map.h"
#include "../../message/include/message.h"

// 全局控制器最大数量
#define FABRIC_CTRL_MAX   16384

void fabric_controller_global_reset(void);

typedef struct FabricController {
	uint32_t          controller_id;   /* 控制器 ID */
	Fabric            *fabric;         /* 关联的 Fabric（软总线）实例 */
	FabricArbiter     *arbiter;        /* 仲裁器，保护总线互斥访问 */
	FabricFunctionMap *function_map;    /* 本控制器管辖的函数映射表 */
} FabricController;

/* ==================== 控制器生命周期 ==================== */

// 创建并初始化控制器，同时将其注册到全局控制器表。
FabricController *fabric_controller_create(Fabric *fabric);

// 销毁控制器并释放相关资源
int fabric_controller_destroy(FabricController *ctrl);

/* ==================== 消息队列操作 ==================== */

// 将消息入队到控制器关联的 Fabric 消息队列（READY 队列）。
int fabric_controller_enqueue(FabricController *ctrl, FabricMsg *msg);

// 从控制器关联的 Fabric 消息队列（READY 队列）取出一条消息
FabricMsg *fabric_controller_dequeue(FabricController *ctrl);

// 路由：通过 DFS 查找拥有目标服务的 Fabric
struct Fabric;
Fabric *fabric_controller_route_dfs(FabricController *ctrl,
				    FabricMsg *msg);

/* ==================== 消息处理 ==================== */

/* TODO: not yet implemented
 * int fabric_controller_process(FabricController *ctrl);
 */

// 控制器处理消息的入口函数：供fabric_send_msg使用
int fabric_controller_handle_request(FabricController *ctrl, FabricMsg *msg);

/* ==================== 全局控制器管理 ==================== */

extern FabricController *g_fabric_controllers[FABRIC_CTRL_MAX];
extern uint32_t g_fabric_controller_count;

// 根据 ID 获取全局控制器实例。
FabricController *fabric_controller_get(uint32_t id);

#endif /* FABRIC_CONTROLLER_H */