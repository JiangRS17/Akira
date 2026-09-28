#include "./include/message.h"
#include "../include/fabric_error.h"

#include <uk/alloc.h>
#include <flexos/isolation.h>
#include <string.h>

// 创建消息并初始化
FabricMsg *fabric_msg_create(uint32_t src_fabric_id,
                             uint64_t service_id,
                             const char *func_name,
                             void *func_ptr,
                             int has_ret,
                             void *ret_ptr,
                             size_t ret_size,
                             int argc,
                             long args[])
{
    FabricMsg *msg = uk_zalloc(flexos_shared_alloc, sizeof(*msg));
	if (!msg) return NULL;

    msg->src_fabric_id = src_fabric_id;
    msg->service_id = service_id;
    msg->func_name = func_name;
    msg->func_ptr = func_ptr;
    msg->has_ret = has_ret;
    msg->ret_ptr = ret_ptr;
    msg->ret_size = ret_size;
    msg->dst_fabric_id = -1; // 目标 Fabric ID 需要在发送消息时设置
    msg->done = 0; // 初始化为未完成
    msg->priority = 0; // 默认优先级为0

    if (argc > 11) argc = 11;
    msg->argc = argc;

    if (args && argc > 0) {
        memcpy(msg->args, args, argc * sizeof(long));
    }

    // @TODO: 暂时不知道什么用
    // msg->response_cb = NULL;
    // msg->cb_ctx = NULL;

    return msg;
}

int fabric_msg_destroy(FabricMsg *msg)
{
	if (!msg)
		return FABRIC_ERROR;

    uk_free(flexos_shared_alloc, msg);
	return FABRIC_SUCCESS;
}


// @TODO: 回调设置
// void fabric_msg_set_callback(FabricMsg *msg,
//                              void (*response_cb)(FabricMsg *, int, void *),
//                              void *ctx)
// {
//     if (!msg) return;
//     msg->response_cb = response_cb;
//     msg->cb_ctx = ctx;
// }

/* [新设计] 栈上初始化消息（消除 malloc/free 开销）
 * 调用者负责在栈上分配 FabricMsg，此函数只负责填充字段 */
void fabric_msg_init_on_stack(FabricMsg *msg,
                              uint32_t src_fabric_id,
                              uint64_t service_id,
                              const char *func_name,
                              void *func_ptr,
                              int has_ret,
                              void *ret_ptr,
                              size_t ret_size,
                              int argc,
                              long args[])
{
    msg->src_fabric_id = src_fabric_id;
    msg->service_id    = service_id;
    msg->func_name     = func_name;
    msg->func_ptr      = func_ptr;
    msg->has_ret       = has_ret;
    msg->ret_ptr       = ret_ptr;
    msg->ret_size      = ret_size;
    msg->dst_fabric_id = (uint32_t)-1;
    msg->done          = 0;
    msg->priority      = 0;

    if (argc > 11) argc = 11;
    msg->argc = argc;

    if (args && argc > 0) {
        memcpy(msg->args, args, argc * sizeof(long));
    } else {
        memset(msg->args, 0, sizeof(msg->args));
    }
}
