#ifndef FABRIC_MESSAGE_H
#define FABRIC_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct FabricMsg {
    uint32_t src_fabric_id;    // 消息发送者所在Fabric
    uint32_t dst_fabric_id;    // 消息目标Fabric
    uint64_t service_id;       // 所请求的目标服务 ID
    const char *func_name;     // 调用的目标函数/服务名
    void *func_ptr;            // 目标函数指针

    int has_ret;               // 是否期望返回值
    void *ret_ptr;             // 返回值存放地址
    size_t ret_size;           // 返回值缓冲区大小（字节）

    int argc;                  // 参数个数
    long args[11];             // 参数列表
    int done;                  // 消息处理完成标志

    uint32_t priority;         // 消息优先级,暂时默认都为0
    

    // 异步回调 @TODO: 需要吗？？？
	// msg：发送的原始消息
	// result：目标组件执行函数后的返回值
	// ctx：用户自定义上下文（比如调用者的结构体或状态指针）
	// 当消息到达目标组件并执行完对应函数后，调用这个回调通知发送者结果。
    // void (*response_cb)(struct FabricMsg *msg, int result, void *ctx);
    // void *cb_ctx;

} FabricMsg;

// 创建消息  用malloc free  淘汰！！！
FabricMsg *fabric_msg_create(uint32_t src_fabric_id,
                             uint64_t service_id,
                             const char *func_name,
                             void *func_ptr,
                             int has_ret,
                             void *ret_ptr,
                             size_t ret_size,
                             int argc,
                             long args[]);

// 销毁消息
int fabric_msg_destroy(FabricMsg *msg);

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
                              long args[]);


// @TODO: 设置回调
// void fabric_msg_set_callback(FabricMsg *msg,
//                              void (*response_cb)(FabricMsg *, int, void *),
//                              void *ctx);

/* 发送消息 */
// int fabric_send_msg(FabricMsg *msg);


#endif /* FABRIC_MESSAGE_H */