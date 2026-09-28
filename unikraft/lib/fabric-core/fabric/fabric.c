#include "./include/fabric.h"
#include "../include/fabric_error.h"
#include "../queue/include/queue.h"
#include "../component/include/component.h"
#include "../bridge/include/bridge.h"
#include "../controller/include/controller.h"
#include "../map/include/map.h"


#include <flexos/isolation.h>
#include <stdio.h>
#include <string.h>

// 初始化全局fabric映射表
Fabric *g_fabrics[MAX_FABRIC] = { NULL };

// 全局fabric计数
static uint32_t g_fabric_id_counter = 0;

int fabric_create_skip_queues = 0;
int fabric_create_skip_component = 0;

/* Clear table + id allocator (benchmarks rebuild topologies of different N). */
void fabric_global_reset(void)
{
	memset(g_fabrics, 0, sizeof(g_fabrics));
	g_fabric_id_counter = 0;
	fabric_controller_global_reset();
}

// 创建并初始化软总线
Fabric *fabric_create() {
    Fabric *fabric;
    uint32_t id;

    fabric = uk_zalloc(flexos_shared_alloc, sizeof(*fabric));
    if (!fabric) { 
        return NULL;
    }

    id = __sync_fetch_and_add(&g_fabric_id_counter, 1);
    if (id >= MAX_FABRIC) {
        uk_free(flexos_shared_alloc, fabric);
        return NULL;
    }
    fabric->fabric_id = id;
    fabric->status = FABRIC_STATUS_INIT;

    // 创建并绑定控制器
    fabric->controller = fabric_controller_create(fabric);
    if (!fabric->controller) {   
        uk_free(flexos_shared_alloc, fabric);
        return NULL;
    }

    // 创建并绑定桥节点
    fabric->bridge = fabric_bridge_create(fabric);
    if (!fabric->bridge) {
        fabric_controller_destroy(fabric->controller);
        uk_free(flexos_shared_alloc, fabric);
        return NULL;
    }

    // 创建并绑定组件节点（目标执行节点需要；纯路由节点可跳过）
    if (fabric_create_skip_component) {
        fabric->component = NULL;
    } else {
        fabric->component = fabric_component_create(fabric);
        if (!fabric->component) {
            fabric_bridge_destroy(fabric->bridge);
            fabric_controller_destroy(fabric->controller);
            uk_free(flexos_shared_alloc, fabric);
            return NULL;
        }
        fabric->component->status = FB_COMP_STATE_RUNNING;
    }

    if (fabric_create_skip_queues) {
        fabric->queues = NULL;
        fabric->queues_count = 0;
    } else {
        fabric->queues = fabric_queues_create();
        if (!fabric->queues) {
            fabric_bridge_destroy(fabric->bridge);
            fabric_controller_destroy(fabric->controller);
            fabric_component_destroy(fabric->component);
            uk_free(flexos_shared_alloc, fabric);
            return NULL;
        }
        fabric->queues_count = 3; // ready, running, waiting
    }

    /* init performance security evaluator */
    // fabric->perfsec_evaluator = NULL; /* @TODO: create perfsec evaluator */

    // @TODO: 这里该不该设置为 READY 因为目前只是创建了一些空的组件和桥
    fabric->status = FABRIC_STATUS_READY;

    g_fabrics[fabric->fabric_id] = fabric;  // 因为之前id自增了
    return fabric;
}

// 获取 fabric_id
uint32_t get_fabric_id(Fabric *fabric) {
    if (!fabric) {
        return FABRIC_ERROR;
    }
    return fabric->fabric_id;
}

// 销毁软总线
int fabric_destroy(Fabric *fabric) {
    uint32_t fabric_id;

    if (!fabric) {
        return FABRIC_ERROR;
    }

    fabric_id = get_fabric_id(fabric);
    fabric->status = FABRIC_STATUS_SHUTDOWN;

    // 销毁控制器
    if (fabric->controller) {
        fabric_controller_destroy(fabric->controller);
    }

    // 销毁桥节点
    if (fabric->bridge) {
        fabric_bridge_destroy(fabric->bridge);
    }

    // 销毁组件
    if (fabric->component) {
        fabric_component_destroy(fabric->component);
    }

    // 消息队列
    if (fabric->queues) {
        for (int i = 0; i < fabric->queues_count; i++) {
            fabric_queue_destroy(fabric->queues[i]);
        }
        uk_free(flexos_shared_alloc, fabric->queues);
        fabric->queues = NULL;
    }


    /* destroy performance security evaluator */
    // if (fabric->perfsec_evaluator) {
    //     fabric_perfsec_evaluator_destroy(fabric->perfsec_evaluator);
    // }

    // 释放fabric内存
    if (fabric_id < MAX_FABRIC) {
        g_fabrics[fabric_id] = NULL;
    }
    uk_free(flexos_shared_alloc, fabric);

    return FABRIC_SUCCESS;
}

// 注册组件节点中的服务
int fabric_register_function(Fabric *fabric, const char *func_name, func_t func_ptr) {
    if (!fabric || !func_name || !func_ptr) {
        return FABRIC_ERROR;
    }

    if (!fabric->controller || !fabric->controller->function_map) {
        return FABRIC_ERROR;
    }

    if (function_map_register(fabric->controller->function_map, func_name, func_ptr) !=
        FABRIC_SUCCESS) {
        return FABRIC_ERROR;
    }

    return FABRIC_SUCCESS;
}


// 注销组件节点中的服务
int fabric_unregister_function(Fabric *fabric, const char *func_name) {
    if (!fabric || !func_name) {
        return FABRIC_ERROR;
    }

    if (!fabric->controller || !fabric->controller->function_map) {
        return FABRIC_ERROR;
    }

    if (function_map_get(fabric->controller->function_map, func_name) == NULL) {
        return FABRIC_ERROR;
    }

    if (function_map_unregister(fabric->controller->function_map, func_name) !=
        FABRIC_SUCCESS) {
        return FABRIC_ERROR;
    }

    return FABRIC_SUCCESS;
}

// register component to controller
// int fabric_register_component(Fabric *fabric, struct FabricComponent *component) {
//     if (!fabric || !component) {
//         return FABRIC_ERROR;
//     }

//     return FABRIC_SUCCESS;
// }

// 注销组件节点
// int fabric_unregister_component(Fabric *fabric, FabricComponent *component) {
//     if (!fabric || !component) {
//         return FABRIC_ERROR;
//     }

//     if(component_unregister(component) != FABRIC_SUCCESS) {
//         return FABRIC_ERROR;
//     }

//     fabric->component = NULL; 

//     return FABRIC_SUCCESS;
// }

// 统一消息发送接口
int fabric_send_message(Fabric *fabric, FabricMsg *msg) {
    if (!fabric || !msg) {
        return FABRIC_ERROR;
    }

    // 由控制器发送消息到目标组件进行处理
    if(fabric_controller_handle_request(fabric->controller, msg) != FABRIC_SUCCESS) {
        return FABRIC_ERROR;
    }

    return FABRIC_SUCCESS;
}


// 统一消息接收接口
int fabric_receive_message(Fabric *fabric, FabricMsg *msg) {
    if (!fabric || !msg) {
        return FABRIC_ERROR;
    }

    FabricComponent *dst_comp = fabric->component;

    if (!dst_comp) {
        return FABRIC_ERROR;
    }

    return fabric_component_receive(dst_comp, msg);
}


// 入队消息
int fabric_enqueue_message(Fabric *fabric, FabricMsg *msg, fabric_queue_type_t queue_type) {
    if (!fabric || !msg) {
        return FABRIC_ERROR;
    }

    FabricQueue *target_queue = fabric->queues[queue_type];
    if (!target_queue) {
        return FABRIC_ERROR;
    }

    return fabric_queue_enqueue(target_queue, msg);
}

// 出队消息
FabricMsg *fabric_dequeue_message(Fabric *fabric, fabric_queue_type_t queue_type) {
    if (!fabric) {
        return NULL;
    }

    FabricQueue *target_queue = fabric->queues[queue_type];
    if (!target_queue) {
        return NULL;
    }

    return fabric_queue_dequeue(target_queue);
}