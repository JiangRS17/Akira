#include "./include/arbiter.h"
#include "../fabric/include/fabric.h"
#include "../include/fabric_error.h"
#include "../controller/include/controller.h"

/* [旧设计] 使用 uk_mutex 实现互斥（操作系统级锁） */
// #include <uk/sched.h>
// #include <uk/alloc.h>

/* [新设计] 使用 CAS 原子操作实现无锁互斥（CPU 硬件级，通过 GCC 内建函数实现） */


void fabric_arbiter_init(FabricArbiter *arbiter)
{
    if (!arbiter)
        return;

    /* [旧设计] 初始化互斥锁 */
    // arbiter->status = FABRIC_ARBITER_FREE;
    // uk_mutex_init(&arbiter->mutex);

    /* [新设计] 初始化原子变量 */
    __atomic_store_n(&arbiter->status, 0, __ATOMIC_RELAXED);
}


fabric_arbiter_status_t fabric_arbiter_get_status(FabricArbiter *arbiter)
{
    if (!arbiter)
        return FABRIC_ARBITER_FREE;

    /* [旧设计] 通过 mutex 读取 */
    // uk_mutex_lock(&arbiter->mutex);
    // s = arbiter->status;
    // uk_mutex_unlock(&arbiter->mutex);

    /* [新设计] 原子读取（一条指令） */
    int s = __atomic_load_n(&arbiter->status, __ATOMIC_RELAXED);
    return (s == 0) ? FABRIC_ARBITER_FREE : FABRIC_ARBITER_BUSY;
}


int fabric_arbiter_request_access(FabricArbiter *arbiter, struct Fabric *fabric)
{
    (void)fabric;

    if (!arbiter)
        return 0;

    /* [旧设计] 通过 mutex 尝试获取 */
    // uk_mutex_lock(&arbiter->mutex);
    // if (arbiter->status == FABRIC_ARBITER_FREE) {
    //     arbiter->status = FABRIC_ARBITER_BUSY;
    //     granted = 1;
    // }
    // uk_mutex_unlock(&arbiter->mutex);

    /* [新设计] CAS 尝试获取（无竞争时 ~5 ns） */
    int expected = 0;
    if (__atomic_compare_exchange_n(&arbiter->status, &expected, 1, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return 1;  /* 成功获取 */

    return 0;  /* 获取失败（被占用） */
}


void fabric_arbiter_release_access(FabricArbiter *arbiter)
{
    if (!arbiter)
        return;

    /* [旧设计] 通过 mutex 释放 */
    // uk_mutex_lock(&arbiter->mutex);
    // if (arbiter->status == FABRIC_ARBITER_BUSY)
    //     arbiter->status = FABRIC_ARBITER_FREE;
    // uk_mutex_unlock(&arbiter->mutex);

    /* [新设计] 原子写（一条指令 ~1 ns） */
    __atomic_store_n(&arbiter->status, 0, __ATOMIC_RELAXED);
}


void fabric_arbiter_wait_for_access(FabricArbiter *arbiter, struct Fabric *fabric)
{
    (void)fabric;
    if (!arbiter)
        return;

    /* [旧设计] 自旋 + yield（每次迭代 ~300-1500 ns） */
    // for (;;) {
    //     uk_mutex_lock(&arbiter->mutex);
    //     if (arbiter->status == FABRIC_ARBITER_FREE) {
    //         arbiter->status = FABRIC_ARBITER_BUSY;
    //         uk_mutex_unlock(&arbiter->mutex);
    //         return;
    //     }
    //     uk_mutex_unlock(&arbiter->mutex);
    //     uk_sched_yield();
    // }

    /* [新设计] CAS 自旋（无竞争时 ~5 ns） */
    int expected = 0;

    /* 快速路径：一次 CAS 尝试 */
    if (__atomic_compare_exchange_n(&arbiter->status, &expected, 1, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        return;

    /* 慢速路径：自旋等待 */
    for (;;) {
        expected = 0;
        if (__atomic_compare_exchange_n(&arbiter->status, &expected, 1, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;
        /* CPU pause 指令：降低功耗，避免总线拥塞 */
        __asm__ volatile("pause");
    }
}