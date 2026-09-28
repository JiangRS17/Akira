/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Costin Lupu <costin.lupu@cs.pub.ro>
 *
 * Copyright (c) 2019, University Politehnica of Bucharest. All rights reserved.
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
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <uk/config.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct arg_wrapper {
	int argc;
	char **argv;
};

/*
 * kraft run / qemu-guest pass kernel parameters (e.g. console=ttyS0) on the
 * command line. uk_libparam only strips args before "--", so drop leftover
 * key=value tokens before handing argv to Redis while keeping argv[0].
 */
static int skip_boot_argv(const char *arg)
{
	if (!arg || !arg[0])
		return 1;
	if (strchr(arg, '='))
		return 1;
	/* Unikraft may pass the kernel path as argv[0]. */
	if (strchr(arg, '/') && !strstr(arg, ".conf"))
		return 1;
	return 0;
}

static void prepare_redis_argv(int argc, char *argv[], struct arg_wrapper *aw)
{
	static char *filtered[CONFIG_LIBUKBOOT_MAXNBARGS];
	int i, n = 0;

	filtered[n++] = "redis-server";

	for (i = 0; i < argc; i++) {
		if (n >= CONFIG_LIBUKBOOT_MAXNBARGS)
			break;
		if (skip_boot_argv(argv[i]))
			continue;
		filtered[n++] = argv[i];
	}

	aw->argc = n;
	aw->argv = filtered;
}


/*
 * Server thread
 */
#if CONFIG_LIBREDIS_SERVER_MAIN_FUNCTION
extern int redis_server_main(int argc, char *argv[]);

void *server_thread_fn(void *arg)
{
	long rc;
	struct arg_wrapper *aw = arg;

	rc = redis_server_main(aw->argc, aw->argv);
	return (void *) rc;
}
#endif

/*
 * Client thread
 */
#if CONFIG_LIBREDIS_CLIENT_MAIN_FUNCTION
extern int redis_client_main(int argc, char *argv[]);

void *client_thread_fn(void *arg)
{
	long rc;
	struct arg_wrapper *aw = arg;

	/* Wait for environment to be ready */
	sleep(2);

	rc = redis_client_main(aw->argc, aw->argv);
	return (void *) rc;
}
#endif

int main(int argc, char *argv[])
{
	int rc, _rc;
	struct arg_wrapper aw;
#if CONFIG_LIBREDIS_SERVER_MAIN_FUNCTION
	pthread_t thread_server;
#endif
#if CONFIG_LIBREDIS_CLIENT_MAIN_FUNCTION
	pthread_t thread_client;
#endif

	prepare_redis_argv(argc, argv, &aw);

	/* Start server */
#if CONFIG_LIBREDIS_SERVER_MAIN_FUNCTION
	rc = pthread_create(&thread_server, NULL, server_thread_fn, &aw);
	if (rc)
		goto out;
#endif

	/* Start client and wait for it to end */
#if CONFIG_LIBREDIS_CLIENT_MAIN_FUNCTION
	rc = pthread_create(&thread_client, NULL, client_thread_fn, &aw);
	if (rc == 0)
		rc = pthread_join(thread_client, NULL);
#endif

	/* Wait for server to end */
#if CONFIG_LIBREDIS_SERVER_MAIN_FUNCTION
	_rc = pthread_join(thread_server, NULL);
	if (_rc && !rc)
		rc = _rc;
out:
#endif

	return rc;
}
