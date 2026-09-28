#include "./include/map.h"
#include "../include/fabric_error.h"
#include <string.h>
#include <uk/alloc.h>
#include <flexos/isolation.h>

uint64_t fabric_service_id_from_name(const char *func_name)
{
    uint64_t hash = 1469598103934665603ULL;
    unsigned char c;

    if (!func_name)
        return 0;

    while ((c = (unsigned char)*func_name++) != 0) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    return hash;
}

static uint32_t hash_ptr(func_t func_ptr)
{
    uintptr_t value = (uintptr_t) func_ptr;

    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;

    return (uint32_t) value;
}

static uint32_t hash_service_id(uint64_t service_id)
{
    service_id ^= service_id >> 33;
    service_id *= 0xff51afd7ed558ccdULL;
    service_id ^= service_id >> 33;
    service_id *= 0xc4ceb9fe1a85ec53ULL;
    service_id ^= service_id >> 33;

    return (uint32_t) service_id;
}

static void service_bucket_remove(FabricFunctionMap *map, FunctionMapEntry *entry)
{
    uint32_t index;
    FunctionMapEntry *node;
    FunctionMapEntry *prev = NULL;

    if (!map || !entry || !entry->service_id)
        return;

    index = hash_service_id(entry->service_id) % map->capacity;
    node = map->service_buckets[index];

    while (node) {
        if (node == entry) {
            if (prev)
                prev->service_next = node->service_next;
            else
                map->service_buckets[index] = node->service_next;
            entry->service_next = NULL;
            return;
        }
        prev = node;
        node = node->service_next;
    }
}

static void service_bucket_insert(FabricFunctionMap *map, FunctionMapEntry *entry)
{
    uint32_t index;

    if (!map || !entry || !entry->service_id)
        return;

    index = hash_service_id(entry->service_id) % map->capacity;
    entry->service_next = map->service_buckets[index];
    map->service_buckets[index] = entry;
}

static void ptr_bucket_remove(FabricFunctionMap *map, FunctionMapEntry *entry)
{
    uint32_t index;
    FunctionMapEntry *node;
    FunctionMapEntry *prev = NULL;

    if (!map || !entry || !entry->func_ptr)
        return;

    index = hash_ptr(entry->func_ptr) % map->capacity;
    node = map->ptr_buckets[index];

    while (node) {
        if (node == entry) {
            if (prev)
                prev->ptr_next = node->ptr_next;
            else
                map->ptr_buckets[index] = node->ptr_next;
            entry->ptr_next = NULL;
            return;
        }
        prev = node;
        node = node->ptr_next;
    }
}

static void ptr_bucket_insert(FabricFunctionMap *map, FunctionMapEntry *entry)
{
    uint32_t index;

    if (!map || !entry || !entry->func_ptr)
        return;

    index = hash_ptr(entry->func_ptr) % map->capacity;
    entry->ptr_next = map->ptr_buckets[index];
    map->ptr_buckets[index] = entry;
}

/* 内部使用的字符串哈希函数 (DJB2 算法) */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    return hash;
}

FabricFunctionMap* function_map_create(uint32_t capacity) {
    if (capacity == 0)
        capacity = 128;
    
    FabricFunctionMap *map = uk_zalloc(flexos_shared_alloc, sizeof(FabricFunctionMap));
    if (!map)
        return NULL;
    
    map->capacity = capacity;
    map->name_buckets = uk_zalloc(flexos_shared_alloc,
                                  capacity * sizeof(FunctionMapEntry *));
    if (!map->name_buckets) {
        uk_free(flexos_shared_alloc, map);
        return NULL;
    }

    map->service_buckets = uk_zalloc(flexos_shared_alloc,
                                     capacity * sizeof(FunctionMapEntry *));
    if (!map->service_buckets) {
        uk_free(flexos_shared_alloc, map->name_buckets);
        uk_free(flexos_shared_alloc, map);
        return NULL;
    }

    map->ptr_buckets = uk_zalloc(flexos_shared_alloc,
                                 capacity * sizeof(FunctionMapEntry *));
    if (!map->ptr_buckets) {
        uk_free(flexos_shared_alloc, map->service_buckets);
        uk_free(flexos_shared_alloc, map->name_buckets);
        uk_free(flexos_shared_alloc, map);
        return NULL;
    }
    return map;
}

void function_map_destroy(FabricFunctionMap *map) {
    if (!map)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        FunctionMapEntry *node = map->name_buckets[i];
        while (node) {
            FunctionMapEntry *next = node->name_next;
            uk_free(flexos_shared_alloc, node);
            node = next;
        }
    }
    uk_free(flexos_shared_alloc, map->service_buckets);
    uk_free(flexos_shared_alloc, map->ptr_buckets);
    uk_free(flexos_shared_alloc, map->name_buckets);
    uk_free(flexos_shared_alloc, map);
}

int function_map_register(FabricFunctionMap *map, const char *func_name, func_t func_ptr) {
    uint64_t service_id;
    uint32_t service_index;

    if (!map || !func_name || !func_ptr)
        return FABRIC_ERROR;

    service_id = fabric_service_id_from_name(func_name);
    if (!service_id)
        return FABRIC_ERROR;
    
    uint32_t index = hash_string(func_name) % map->capacity;

    service_index = hash_service_id(service_id) % map->capacity;
    FunctionMapEntry *service_node = map->service_buckets[service_index];
    while (service_node) {
        if (service_node->service_id == service_id &&
            strcmp(service_node->func_name, func_name) != 0)
            return FABRIC_ERROR;
        service_node = service_node->service_next;
    }
    
    /* 1. 检查是否存在同名函数，存在则直接覆盖 (Update 语义) */
    FunctionMapEntry *node = map->name_buckets[index];
    while (node) {
        if (strcmp(node->func_name, func_name) == 0) {
            node->service_id = service_id;
            if (node->func_ptr != func_ptr) {
                ptr_bucket_remove(map, node);
                node->func_ptr = func_ptr;
                ptr_bucket_insert(map, node);
            }
            return FABRIC_SUCCESS;
        }
        node = node->name_next;
    }
    
    /* 2. 不存在，插入新节点 (Insert 语义，头插法) */
    FunctionMapEntry *new_node = uk_zalloc(flexos_shared_alloc, sizeof(FunctionMapEntry));
    if (!new_node)
        return FABRIC_ERROR;

    /* func_name is fixed-length storage in node */
    strncpy(new_node->func_name, func_name, sizeof(new_node->func_name) - 1);
    new_node->func_name[sizeof(new_node->func_name) - 1] = '\0';
    
    new_node->service_id = service_id;
    new_node->func_ptr = func_ptr;
    new_node->name_next = map->name_buckets[index];
    map->name_buckets[index] = new_node;
    service_bucket_insert(map, new_node);
    ptr_bucket_insert(map, new_node);
    
    return FABRIC_SUCCESS;
}

func_t function_map_get(FabricFunctionMap *map, const char *func_name) {
    if (!map || !func_name)
        return NULL;

    uint32_t index = hash_string(func_name) % map->capacity;
    FunctionMapEntry *node = map->name_buckets[index];

    while (node) {
        if (strcmp(node->func_name, func_name) == 0)
            return node->func_ptr;
        node = node->name_next;
    }

    return NULL;
}

func_t function_map_get_by_service_id(FabricFunctionMap *map, uint64_t service_id)
{
    if (!map || !service_id)
        return NULL;

    uint32_t index = hash_service_id(service_id) % map->capacity;
    FunctionMapEntry *node = map->service_buckets[index];

    while (node) {
        if (node->service_id == service_id)
            return node->func_ptr;
        node = node->service_next;
    }

    return NULL;
}

func_t function_map_get_by_ptr(FabricFunctionMap *map, func_t func_ptr)
{
    if (!map || !func_ptr)
        return NULL;

    uint32_t index = hash_ptr(func_ptr) % map->capacity;
    FunctionMapEntry *node = map->ptr_buckets[index];

    while (node) {
        if (node->func_ptr == func_ptr)
            return node->func_ptr;
        node = node->ptr_next;
    }

    return NULL;
}

int function_map_unregister(FabricFunctionMap *map, const char *func_name)
{
    if (!map || !func_name)
        return FABRIC_ERROR;

    uint32_t index = hash_string(func_name) % map->capacity;
    FunctionMapEntry *node = map->name_buckets[index];
    FunctionMapEntry *prev = NULL;

    while (node) {
        if (strcmp(node->func_name, func_name) == 0) {
            service_bucket_remove(map, node);
            ptr_bucket_remove(map, node);
            if (prev)
                prev->name_next = node->name_next;
            else
                map->name_buckets[index] = node->name_next;

            uk_free(flexos_shared_alloc, node);
            return FABRIC_SUCCESS;
        }
        prev = node;
        node = node->name_next;
    }

    return FABRIC_ERROR;
}

bool function_map_contains(FabricFunctionMap *map, const char *func_name) {
    return function_map_get(map, func_name) != NULL;
}

bool function_map_contains_service_id(FabricFunctionMap *map, uint64_t service_id)
{
    return function_map_get_by_service_id(map, service_id) != NULL;
}

bool function_map_contains_ptr(FabricFunctionMap *map, func_t func_ptr)
{
    return function_map_get_by_ptr(map, func_ptr) != NULL;
}