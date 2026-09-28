#include "./include/queue.h"
#include "../include/fabric_error.h"
#include "../message/include/message.h"

#include <uk/alloc.h>
#include <stdlib.h>
#include <uk/mutex.h>
#include <flexos/isolation.h>

// id 计数器
static uint32_t g_queue_id_counter = 0;

// 创建指定类型的队列
FabricQueue *fabric_queue_create(fabric_queue_type_t type) {
    FabricQueue *q = uk_zalloc(flexos_shared_alloc, sizeof(*q));
    if (!q) {
        return NULL;
    }

    q->queue_id = __sync_fetch_and_add(&g_queue_id_counter, 1);
    q->priority = 0;                // default priority
    q->type = type;                 // set the queue type
    q->length = 0;
    q->head = NULL;
    q->tail = NULL;

    uk_mutex_init(&q->mutex);

    return q;
}

// 创建三条队列
FabricQueue **fabric_queues_create() {
    FabricQueue **queues = uk_zalloc(flexos_shared_alloc, sizeof(FabricQueue *) * 3);
    if (!queues) {
        return NULL;
    }

    queues[FABRIC_QUEUE_TYPE_READY] = fabric_queue_create(FABRIC_QUEUE_TYPE_READY);
    queues[FABRIC_QUEUE_TYPE_RUNNING] = fabric_queue_create(FABRIC_QUEUE_TYPE_RUNNING);
    queues[FABRIC_QUEUE_TYPE_WAITING] = fabric_queue_create(FABRIC_QUEUE_TYPE_WAITING);

    if (!queues[FABRIC_QUEUE_TYPE_READY] || !queues[FABRIC_QUEUE_TYPE_RUNNING] || !queues[FABRIC_QUEUE_TYPE_WAITING]) {
        for (int i = 0; i < 3; i++) {
            if (queues[i]) {
                fabric_queue_destroy(queues[i]);
            }
        }
        uk_free(flexos_shared_alloc, queues);
        return NULL;
    }

    return queues;
}


// 销毁队列
int fabric_queue_destroy(FabricQueue *q) {
    if (!q) {
        return FABRIC_ERROR;
    }

    uk_mutex_lock(&q->mutex);

    // 释放队列中所有的节点
    FabricQueueNode *current = q->head;
    q->head = NULL;
    q->tail = NULL;
    q->length = 0;

    uk_mutex_unlock(&q->mutex);

    while (current) {
        FabricQueueNode *next = current->next;
        /* @TODO: free message if no external owner */
        uk_free(flexos_shared_alloc, current);

        current = next;
    }

    uk_free(flexos_shared_alloc, q);
    return FABRIC_SUCCESS;
}

// 入队（按优先级）
int fabric_queue_enqueue(FabricQueue *q, struct FabricMsg *msg) {
    if (!q || !msg) {
        return FABRIC_ERROR;
    }

    uk_mutex_lock(&q->mutex); // 加锁

    FabricQueueNode *node = uk_zalloc(flexos_shared_alloc, sizeof(*node));
    if (!node) {
        uk_mutex_unlock(&q->mutex);
        return FABRIC_ERROR;
    }

    node->msg = msg;
    node->next = NULL;

    if (!q->head) {
        q->head = node;
        q->tail = node;
    } else if (msg->priority > q->head->msg->priority) {
        node->next = q->head;
        q->head = node;
    } else {
        FabricQueueNode *cur = q->head;

        while (cur->next && cur->next->msg->priority >= msg->priority) {
            cur = cur->next;
        }

        node->next = cur->next;
        cur->next = node;
        if (!node->next) {
            q->tail = node;
        }
    }

    q->length++;

    uk_mutex_unlock(&q->mutex);
    return FABRIC_SUCCESS;
}

// 出队消息
struct FabricMsg *fabric_queue_dequeue(FabricQueue *q) {
    if (!q) {
        return NULL;
    }

    uk_mutex_lock(&q->mutex);

    if (!q->head) {
        uk_mutex_unlock(&q->mutex);
        return NULL;
    }

    FabricQueueNode *node = q->head;
    struct FabricMsg *msg = node->msg;
    q->head = node->next;
    if (!q->head) {
        q->tail = NULL;
    }
    q->length--;

    uk_free(flexos_shared_alloc, node);
    uk_mutex_unlock(&q->mutex);
    return msg;
}

/* peek at the first message in the queue */
struct FabricMsg *fabric_queue_peek(FabricQueue *q) {
    if (!q) {
        return NULL;
    }

    uk_mutex_lock(&q->mutex);

    if (!q->head) {
        uk_mutex_unlock(&q->mutex);
        return NULL;
    }

    FabricQueueNode *node = q->head;
    struct FabricMsg *msg = node->msg;

    uk_mutex_unlock(&q->mutex);
    return msg;
}

/* check if queue is empty */
int fabric_queue_is_empty(FabricQueue *q) {
    if (!q) {
        return 1;  // treat NULL queue as empty
    }

    uk_mutex_lock(&q->mutex);
    int empty = (q->head == NULL);
    uk_mutex_unlock(&q->mutex);

    return empty;
}

/* priority operations */
int fabric_queue_set_priority(FabricQueue *q, uint32_t priority) {
    if (!q) {
        return FABRIC_ERROR;
    }

    uk_mutex_lock(&q->mutex);
    q->priority = priority;
    uk_mutex_unlock(&q->mutex);
    return FABRIC_SUCCESS;
}

uint32_t fabric_queue_get_priority(FabricQueue *q) {
    if (!q) {
        return FABRIC_ERROR;
    }

    uk_mutex_lock(&q->mutex);
    uint32_t priority = q->priority;
    uk_mutex_unlock(&q->mutex);

    return priority;
}

int fabric_queue_set_type(FabricQueue *q, fabric_queue_type_t type) {
    if (!q) {
        return FABRIC_ERROR;
    }

    uk_mutex_lock(&q->mutex);
    q->type = type;
    uk_mutex_unlock(&q->mutex);
    return FABRIC_SUCCESS;
}

fabric_queue_type_t fabric_queue_get_type(FabricQueue *q) {
    if (!q) {
        return FABRIC_QUEUE_TYPE_READY;  // default type
    }

    uk_mutex_lock(&q->mutex);
    fabric_queue_type_t type = q->type;
    uk_mutex_unlock(&q->mutex);

    return type;
}