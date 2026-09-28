#ifndef FABRIC_ARBITER_H
#define FABRIC_ARBITER_H


#include <stdint.h>
/* [旧设计] 使用操作系统互斥锁（uk_mutex） */
// #include <uk/mutex.h>
// #include <stdatomic.h>  /* newlib 的 stdatomic.h 在 GCC 路径下 atomic_init 有 bug，改用 GCC 内建函数 */


// 前置声明
struct FabricController;
struct FabricMsg;
struct Fabric;

/* 仲裁器状态：对应软总线占用情况 */
typedef enum {
	FABRIC_ARBITER_FREE = 0,
	FABRIC_ARBITER_BUSY,
	FABRIC_ARBITER_WAITING
} fabric_arbiter_status_t;

typedef int (*fabric_arbiter_fn_t)(struct FabricController *ctrl,
								   struct FabricMsg *msg);

/* [旧设计] 使用 uk_mutex 实现互斥 */
// typedef struct FabricArbiter {
// 	fabric_arbiter_status_t status;
// 	struct uk_mutex mutex;
// } FabricArbiter;

/* [新设计] 使用 CAS 原子操作实现无锁互斥（通过 GCC 内建函数实现） */
typedef struct FabricArbiter {
	int status;    /* 0 = FREE, 1 = BUSY，通过 __atomic_* 内建函数访问 */
} FabricArbiter;


void fabric_arbiter_init(FabricArbiter *arbiter);
fabric_arbiter_status_t fabric_arbiter_get_status(FabricArbiter *arbiter);
int fabric_arbiter_request_access(FabricArbiter *arbiter, struct Fabric *fabric);
void fabric_arbiter_release_access(FabricArbiter *arbiter);
void fabric_arbiter_wait_for_access(FabricArbiter *arbiter, struct Fabric *fabric);

#endif /* FABRIC_ARBITER_H */
