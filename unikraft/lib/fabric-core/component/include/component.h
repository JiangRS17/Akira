#ifndef _FABRIC_COMPONENT_H_
#define _FABRIC_COMPONENT_H_

#include <stddef.h>
#include <stdint.h>

struct Fabric;
struct FabricMsg;
struct FabricController;

typedef uint32_t fb_comp_id_t;
typedef uint32_t fb_capability_t;
// typedef uint32_t fb_iso_domain_t;


// 组件状态
typedef enum {
	FB_COMP_STATE_INIT = 0,
	FB_COMP_STATE_REGISTERED,
	FB_COMP_STATE_RUNNING,
	FB_COMP_STATE_ISOLATED,
	FB_COMP_STATE_STOPPED,
	FB_COMP_STATE_UNREGISTERED,
} fb_comp_status_t;


typedef struct FabricComponent {
    // 组件id
	fb_comp_id_t id;

    // @ TODO: 暂时不知道什么用
    fb_capability_t capabilities;

    // 组件状态
	fb_comp_status_t status;

    // 组件所在的隔离域 @TODO: 暂时不实现
	// fb_iso_domain_t iso_domain;

    // 组件所在的软总线实例
	struct Fabric *fabric;

    // 组件所在的软总线控制器实例
	struct FabricController *controller;

    // @TODO: 暂时不知道什么用
	// void *business;

} FabricComponent;

// 创建组件并且初始化
FabricComponent *fabric_component_create(struct Fabric *fabric);

// 摧毁组件
int fabric_component_destroy(FabricComponent *comp);

// int component_register(FabricComponent *comp, struct Fabric *fabric);
// 注销组件
int fabric_component_unregister(FabricComponent *comp);

// 组件消息接收接口
int fabric_component_receive(FabricComponent *comp, struct FabricMsg *msg);


/* switch isolation domain */
// int component_switch_isolation(FabricComponent *comp, fb_iso_domain_t new_domain); 

#endif