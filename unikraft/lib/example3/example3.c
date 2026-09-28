#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example3/isolated.h>
#include <example4/isolated.h>

__attribute__((noinline))
void example3_empty(void)
{
	asm volatile("");
}

__attribute__((noinline))
void e3c1(void)
{
	asm volatile("");
	fabric_gate(2, example4_empty);
}

__attribute__((noinline))
void e3c2(void)
{
	asm volatile("");
	fabric_gate(2, e4c1);
}
