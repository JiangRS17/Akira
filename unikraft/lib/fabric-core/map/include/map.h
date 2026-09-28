#ifndef _MAP_H_
#define _MAP_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * 仅维护函数名 -> 函数指针映射。
 */
typedef void (*func_t)(void);


typedef struct FunctionMapEntry {
    char func_name[64];   // 函数名，最长 63 字符 + 1 字符结尾
    uint64_t service_id;  // service_id 用于func_map 查询目标服务
    func_t func_ptr;      // 函数指针
    struct FunctionMapEntry *name_next; // 名字哈希桶冲突链
    struct FunctionMapEntry *service_next; // 服务 ID 哈希桶冲突链
    struct FunctionMapEntry *ptr_next; // 指针哈希桶冲突链
} FunctionMapEntry;

typedef struct FabricFunctionMap {
    uint32_t capacity; // 哈希桶数量
    FunctionMapEntry **name_buckets; // 名字哈希桶数组
    FunctionMapEntry **service_buckets; // 服务 ID 哈希桶数组
    FunctionMapEntry **ptr_buckets; // 指针哈希桶数组
} FabricFunctionMap;

// 创建 / 销毁 函数映射表
FabricFunctionMap* function_map_create(uint32_t capacity);
void function_map_destroy(FabricFunctionMap *map);

// 注册函数 (等同于 map["func_name"] = func_ptr)
int function_map_register(FabricFunctionMap *map, const char *func_name, func_t func_ptr);

// 注销函数
int function_map_unregister(FabricFunctionMap *map, const char *func_name);

// 查询函数指针 (等同于 map["func_name"]，不存在返回 NULL)
func_t function_map_get(FabricFunctionMap *map, const char *func_name);

// 按 service_id 查询函数指针。
func_t function_map_get_by_service_id(FabricFunctionMap *map, uint64_t service_id);

// 按函数指针查询/匹配，避免跨 compartment 读取函数名字符串。
func_t function_map_get_by_ptr(FabricFunctionMap *map, func_t func_ptr);

// 判断是否含有某个函数
bool function_map_contains(FabricFunctionMap *map, const char *func_name);
bool function_map_contains_service_id(FabricFunctionMap *map, uint64_t service_id);
bool function_map_contains_ptr(FabricFunctionMap *map, func_t func_ptr);

/* 由函数名派生稳定 service_id，避免热路径字符串匹配。 */
uint64_t fabric_service_id_from_name(const char *func_name);


#endif /* _FABRIC_FUNC_MAP_H_ */