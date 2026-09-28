#include <flexos/isolation.h>
#include <example8/isolated.h>

__attribute__((noinline))
void example8_empty(void)
{
	asm volatile("");
}
