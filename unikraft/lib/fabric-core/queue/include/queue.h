#ifndef FABRIC_QUEUE_H
#define FABRIC_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <uk/mutex.h>


// 前向声明
struct FabricMsg;


// 消息队列节点
typedef struct FabricQueueNode {
    struct FabricMsg *msg;
    struct FabricQueueNode *next;
} FabricQueueNode;


// 队列类型：就绪态 运行态 等待态
typedef enum {
    FABRIC_QUEUE_TYPE_READY = 0, 
    FABRIC_QUEUE_TYPE_RUNNING = 1, 
    FABRIC_QUEUE_TYPE_WAITING = 2, 
} fabric_queue_type_t;


// 消息队列数据结构
typedef struct FabricQueue {
    uint32_t queue_id;          // 消息队列 id
    uint32_t priority;          // 消息队列优先级
    fabric_queue_type_t type;   // 队列类型：就绪态 运行态 等待态
    uint32_t length;            // 队列中消息的数量

    struct uk_mutex mutex;      // 互斥锁

    FabricQueueNode *head;      // 队头
    FabricQueueNode *tail;      // 队尾
} FabricQueue;


/****************** 队列操作 ******************/
// 创建消息队列
FabricQueue *fabric_queue_create(fabric_queue_type_t type); // 指定类型
FabricQueue **fabric_queues_create();  // 创建三条队列：就绪 运行 等待

// 销毁指定队列
int fabric_queue_destroy(FabricQueue *q);

// 指定队列入队消息
int fabric_queue_enqueue(FabricQueue *q, struct FabricMsg *msg);

// 从指定队列出队消息
struct FabricMsg *fabric_queue_dequeue(FabricQueue *q);

// 查看指定队列队头（不出队）
struct FabricMsg *fabric_queue_peek(FabricQueue *q);

// 队列是否为空
int fabric_queue_is_empty(FabricQueue *q);

// 队列优先级操作
int fabric_queue_set_priority(FabricQueue *q, uint32_t priority);
uint32_t fabric_queue_get_priority(FabricQueue *q);

// 队列类型设置/获取
int fabric_queue_set_type(FabricQueue *q, fabric_queue_type_t type);
fabric_queue_type_t fabric_queue_get_type(FabricQueue *q);



#endif // FABRIC_QUEUE_H