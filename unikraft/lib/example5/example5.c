#include <flexos/isolation.h>
#include <example5/isolated.h>

__attribute__((noinline))
void example5_empty(void)
{
	asm volatile("");
}
