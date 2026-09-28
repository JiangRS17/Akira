/*
 * Stub for Intel PKU intrinsics in standalone test.
 * On real hardware, these use the rdpkru/wrpkru x86 instructions.
 */
#ifndef _FLEXOS_INTELPKU_STUB_H_
#define _FLEXOS_INTELPKU_STUB_H_

#include <stdint.h>

__attribute__((always_inline)) static inline uint32_t rdpkru(void)
{
#if defined(__x86_64__) || defined(__i386__)
    uint32_t res;
    asm volatile(
        "xor %%ecx, %%ecx;"
        "rdpkru;"
        "movl %%eax, %0"
        : "=r"(res) :: "rax", "rdx", "ecx");
    return res;
#else
    return 0;
#endif
}

__attribute__((always_inline)) static inline void wrpkru(uint32_t val)
{
#if defined(__x86_64__) || defined(__i386__)
    asm volatile(
        "xor %%ecx, %%ecx\n\t"
        "xor %%edx, %%edx\n\t"
        "wrpkru\n\t"
        "lfence"
        : : "a"(val) : "ecx", "edx");
#else
    (void)val;
#endif
}

#endif /* _FLEXOS_INTELPKU_STUB_H_ */
