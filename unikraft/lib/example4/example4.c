#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example4/isolated.h>
#include <example5/isolated.h>

__attribute__((noinline))
void example4_empty(void)
{
	asm volatile("");
}

__attribute__((noinline))
void e4c1(void)
{
	asm volatile("");
	fabric_gate(3, example5_empty);
}
