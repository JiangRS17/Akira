#include <flexos/isolation.h>
#include <example1/isolated.h>

__attribute__((noinline))
void example1_empty(void)
{
	asm volatile("");
}
