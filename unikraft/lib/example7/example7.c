#include <flexos/isolation.h>
#include <example7/isolated.h>

__attribute__((noinline))
void example7_empty(void)
{
	asm volatile("");
}
