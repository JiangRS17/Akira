#include <flexos/isolation.h>
#include <example6/isolated.h>

__attribute__((noinline))
void example6_empty(void)
{
	asm volatile("");
}
