#ifndef FABRIC_H
#define FABRIC_H

#include <stdint.h>
#include <stddef.h>
#include "../../queue/include/queue.h"


/* Soft-bus table size (grid: layers×libs_per_layer up to ~6×2000). */
#define MAX_FABRIC 16384

void fabric_global_reset(void);

// 前向声明
struct FabricQueue;
struct FabricController;
// struct FabricPerfSecEvaluator;
struct FabricBridge;
struct FabricComponent;
struct FabricMsg;


// 软总线状态
typedef enum {
    FABRIC_STATUS_INIT   = 0,
    FABRIC_STATUS_READY  = 1,
    FABRIC_STATUS_ERROR  = 2,
    FABRIC_STATUS_SHUTDOWN = 3
} fabric_status_t;


typedef void (*func_t)(void);


typedef struct Fabric{

    
    uint32_t fabric_id;      // 软总线 id
    fabric_status_t status;  // 软总线状态

    struct FabricQueue **queues;  // 软总线中的消息队列
    uint32_t queues_count;        // 软总线中的消息队列数量

    struct FabricController *controller;  // 软总线控制器

    // @TODO: 性能安全评估器
    // struct FabricPerfSecEvaluator *perfsec_evaluator;

    struct FabricBridge *bridge;  // 桥节点

    struct FabricComponent *component;  // 软总线所注册的组件

} Fabric;

// 软总线全局映射表:  fabric_id ----> *fabric  
extern Fabric *g_fabrics[MAX_FABRIC];  // 此处避免每一个软总线都维护一个映射表，这个映射表是全局的

/*
 * Scalability benches: skip unused per-fabric queues (hot path does not use them).
 * Saves 3 queue objects + mutexes per fabric so large K (1000–2000) can fit.
 */
extern int fabric_create_skip_queues;
/* Skip component when node only participates in routing (DFS), not execution. */
extern int fabric_create_skip_component;

// 创建软总线并初始化
Fabric *fabric_create();

// 获取软总线id
uint32_t get_fabric_id(Fabric *fabric);

// 销毁软总线
int fabric_destroy(Fabric *fabric);

// 组件管理
// int fabric_register_component(Fabric *fabric, struct FabricComponent *component);
// int fabric_unregister_component(Fabric *fabric, struct FabricComponent *component);

/* message management */
int fabric_send_message(Fabric *fabric, struct FabricMsg *msg);
int fabric_receive_message(Fabric *fabric, struct FabricMsg *msg);
int fabric_enqueue_message(Fabric *fabric, struct FabricMsg *msg, fabric_queue_type_t queue_type);
struct FabricMsg *fabric_dequeue_message(Fabric *fabric, fabric_queue_type_t queue_type);

/* function register */
int fabric_register_function(Fabric *fabric, const char *func_name, func_t func_ptr);
int fabric_unregister_function(Fabric *fabric, const char *func_name);

/* @TODO: perf & sec evaluation functions */

#endif /* FABRIC_H */