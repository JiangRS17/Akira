#ifndef _FABRIC_GATE_H_
#define _FABRIC_GATE_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <flexos/literals.h>
#include "../../map/include/map.h"

typedef long (*fabric_fn_ret_t)(long, long, long, long, long, long, long, long,
                                long, long, long);
typedef void (*fabric_fn_void_t)(long, long, long, long, long, long, long, long,
                                 long, long, long);

struct FabricMsg;

struct fabric_entry {
    const char *name;
    void *ptr;
};

/* used in auto registration */
// #define EXPORT_FABRIC_SYMBOL(comp, fn)
//     static const struct fabric_entry __fabric_entry_##comp##_##fn
//     __attribute__((section("fabric_table_" #comp), used)) = { #fn, (void *)fn }


void fabric_start(uint32_t src_fabric, struct FabricMsg *msg);

void _fabric_do_gate(uint32_t src_fabric, const char *name,
                     void *direct_fn, void *ret_ptr, size_t ret_size,
                     int has_ret, int argc, ...);

/* Like _fabric_do_gate, but skips DFS and targets dst_fabric_id directly.
 * Used by domain-RR benches so routing cost stays O(1) while MPK dst key changes.
 */
void _fabric_do_gate_dst(uint32_t src_fabric, uint32_t dst_fabric,
			 const char *name, void *direct_fn, void *ret_ptr,
			 size_t ret_size, int has_ret, int argc, ...);

#define FABRIC_SAFE_ARG(x) ((long)(intptr_t)(x))

#define _FABRIC_GET_12TH(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, N, ...) N
#undef COUNT_ARGUMENTS
#define COUNT_ARGUMENTS(...) _FABRIC_GET_12TH(dummy, ## __VA_ARGS__, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define FABRIC_PASS_0() 0
#define FABRIC_PASS_1(a1) FABRIC_SAFE_ARG(a1)
#define FABRIC_PASS_2(a1, a2) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2)
#define FABRIC_PASS_3(a1, a2, a3) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3)
#define FABRIC_PASS_4(a1, a2, a3, a4) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4)
#define FABRIC_PASS_5(a1, a2, a3, a4, a5) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5)
#define FABRIC_PASS_6(a1, a2, a3, a4, a5, a6) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5), FABRIC_SAFE_ARG(a6)
#define FABRIC_PASS_7(a1, a2, a3, a4, a5, a6, a7) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5), FABRIC_SAFE_ARG(a6), FABRIC_SAFE_ARG(a7)
#define FABRIC_PASS_8(a1, a2, a3, a4, a5, a6, a7, a8) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5), FABRIC_SAFE_ARG(a6), FABRIC_SAFE_ARG(a7), FABRIC_SAFE_ARG(a8)
#define FABRIC_PASS_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5), FABRIC_SAFE_ARG(a6), FABRIC_SAFE_ARG(a7), FABRIC_SAFE_ARG(a8), FABRIC_SAFE_ARG(a9)
#define FABRIC_PASS_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5), FABRIC_SAFE_ARG(a6), FABRIC_SAFE_ARG(a7), FABRIC_SAFE_ARG(a8), FABRIC_SAFE_ARG(a9), FABRIC_SAFE_ARG(a10)
#define FABRIC_PASS_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) FABRIC_SAFE_ARG(a1), FABRIC_SAFE_ARG(a2), FABRIC_SAFE_ARG(a3), FABRIC_SAFE_ARG(a4), FABRIC_SAFE_ARG(a5), FABRIC_SAFE_ARG(a6), FABRIC_SAFE_ARG(a7), FABRIC_SAFE_ARG(a8), FABRIC_SAFE_ARG(a9), FABRIC_SAFE_ARG(a10), FABRIC_SAFE_ARG(a11)
#define _FABRIC_CAT(a, b) a##b
#define FABRIC_CAT(a, b) _FABRIC_CAT(a, b)
#define FABRIC_PASS_ARGS(...) FABRIC_CAT(FABRIC_PASS_, COUNT_ARGUMENTS(__VA_ARGS__))(__VA_ARGS__)

#define _EFABRIC_GATE(N, src_fabric, func_name, ...) \
    _fabric_do_gate((src_fabric), FLEXOS_SHARED_LITERAL(#func_name), \
                    (void *)(func_name), NULL, 0, 0, (N), \
                    FABRIC_PASS_ARGS(__VA_ARGS__))

#define _EFABRIC_GATE_R(N, src_fabric, retval, func_name, ...) \
    _fabric_do_gate((src_fabric), FLEXOS_SHARED_LITERAL(#func_name), \
                    (void *)(func_name), (void *)&(retval), sizeof(retval), 1, \
                    (N), FABRIC_PASS_ARGS(__VA_ARGS__))

#define fabric_gate(src_fabric, func_name, ...) \
    _EFABRIC_GATE(COUNT_ARGUMENTS(__VA_ARGS__), (src_fabric), func_name, ## __VA_ARGS__)

#define fabric_gate_r(src_fabric, retval, func_name, ...) \
    _EFABRIC_GATE_R(COUNT_ARGUMENTS(__VA_ARGS__), (src_fabric), retval, func_name, ## __VA_ARGS__)

#endif /* _FABRIC_GATE_H_ */