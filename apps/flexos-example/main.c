/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2020-2021, Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <flexos/isolation.h>
#include <flexos/example/isolated.h>
#include <uk/alloc.h>
#include <stdlib.h>

#define EXIT_EARLY 0

/* In the future we want to have all flexos_gate and flexos_gate_r inserted
 * automatically; it will only be a matter of adding flexos_whitelist compiler
 * annotations and this can be automated via static analysis too (conservatively
 * of course, but seems to behave OK in practice, see Cali).
 */

void check_success(int ret)
{
	if (!ret) {
		flexos_gate(libc, printf, "SUCCESS!\n");
	} else {
		flexos_gate(libc, printf, "FAILURE\n");
		exit(ret);
	}
}

/* static buffer that we pass to function1: have to be shared */
static char static_buf[32]  __attribute__((flexos_whitelist));
static char static_buf2[32] __attribute__((flexos_whitelist));

/* a private static buffer: only accessible from this compartment */
static char static_app_secret[32];

int main(int __unused argc, char __unused *argv[])
{
	int ret;

	static_buf[0] =  'A';
	static_buf2[0] = 'B';

	/* shared stack integer: just as an example */
	int stack_int __attribute__((flexos_whitelist)) = 21;
	int stack_int2 __attribute__((flexos_whitelist)) = 42;
	int stack_int3 __attribute__((flexos_whitelist));
	stack_int3 = 84;
	int stack_int4 __attribute__((flexos_whitelist));
	stack_int4 = 168;

	/* stack buffers that we pass to function1: have to be shared */
	char stack_buf[32] __attribute__((flexos_whitelist));
	stack_buf[0] = 'C';

	/* this second definition makes sure that our transformations can be applied
	 * multiple times in the same function. */
	char stack_buf2[32] __attribute__((flexos_whitelist));
	stack_buf2[0] = 'D';

	/* heap buffers that we pass to function1: have to be shared */
	char *heap_buf = flexos_malloc_whitelist(32, libflexosexample);
	heap_buf[0] = 'E';

	char *heap_buf2 = flexos_malloc_whitelist(32, libflexosexample);
	heap_buf2[0] = 'F';

	static_app_secret[20] = 'z';

	/* This call to return is here just to test that stack-to-heap
	 * transformations also add free() calls in mid-function return calls. */
	if (EXIT_EARLY) {
		return 0;
	}

	/* a private stack buffer: only accessible from this compartment */
	static char stack_app_secret[32];
	stack_app_secret[10] = static_app_secret[20];

	/* a private heap buffer: only accessible from this compartment */
	char *heap_app_secret = uk_malloc(uk_alloc_get_default(), 32);
	heap_app_secret[10] = stack_app_secret[10];

	flexos_gate(libc, printf, "Cross-compartment call with *statically* allocated shared data: ");
	flexos_gate_r(libflexosexample, ret, function1, static_buf);
	flexos_gate_r(libflexosexample, ret, function1, static_buf2);
	/* note: no gates for check_success, this is a micro-lib internal call */
	check_success(ret);

	flexos_gate(libc, printf, "Cross-compartment call with *stack* allocated shared data: ");
	flexos_gate_r(libflexosexample, ret, function1, stack_buf);
	flexos_gate_r(libflexosexample, ret, function1, stack_buf2);
	check_success(ret);

	flexos_gate(libc, printf, "Cross-compartment call with *heap* allocated shared data: ");
	flexos_gate_r(libflexosexample, ret, function1, heap_buf);
	flexos_gate_r(libflexosexample, ret, function1, heap_buf2);
	check_success(ret);

	flexos_gate(libc, printf, "Printing my shared stack ints: %d %d %d %d\n",
		stack_int, stack_int2, stack_int3, stack_int4);

	flexos_gate(libc, printf, "Now executing an invalid cross-compartment access: THIS SHOULD CRASH NOW.\n");
	/* static_app_secret should not be accessible from the other domain */
	flexos_gate_r(libflexosexample, ret, function1, static_app_secret);

	/* same as before */
	if (EXIT_EARLY) {
		return 0;
	}

	flexos_gate(libc, printf, "************* BUG *************");
	flexos_gate(libc, printf, "We should have crashed already!");
	flexos_gate(libc, printf, "*******************************");

	/* no return call, but free() calls should still be inserted if
	 * stack-to-heap conversion is enabled. */
}
