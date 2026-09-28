#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example2/isolated.h>
#include <example3/isolated.h>

/*
 * Fabric IDs: fabric-1=0 … fabric-5=4
 * Short chain names keep FNV cost comparable across hop counts.
 *   e2c1: 2 hops from app  (fab2→3)
 *   e2c2: 3 hops           (fab2→3→4)
 *   e2c3: 4 hops           (fab2→3→4→5)
 *
 * Direct-vs-fabric ablation helpers (Part D):
 *   e2_call_e3_direct — example2 → example3_empty via direct call
 *   e2_call_e3_msg    — example2 → example3_empty via fabric_gate
 *   e2c1              — example2 → example3 via fabric_gate (message)
 */

int example2_secret_value = 42;

__attribute__((noinline))
void example2_empty(void)
{
	asm volatile("");
}

/* Same-domain style: lib2 → lib3 empty via direct call (no soft-bus). */
__attribute__((noinline))
void e2_call_e3_direct(void)
{
	asm volatile("");
	example3_empty();
}

/* Message style: lib2 → lib3 empty via fabric_gate (microkernel-like). */
__attribute__((noinline))
void e2_call_e3_msg(void)
{
	asm volatile("");
	fabric_gate(1, example3_empty);
}

__attribute__((noinline))
void e2c1(void)
{
	asm volatile("");
	fabric_gate(1, example3_empty);
}

__attribute__((noinline))
void e2c2(void)
{
	asm volatile("");
	fabric_gate(1, e3c1);
}

__attribute__((noinline))
void e2c3(void)
{
	asm volatile("");
	fabric_gate(1, e3c2);
}
