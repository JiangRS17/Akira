/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 * Copyright (c) 2019, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
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
#include <flexos/isolation.h>
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/inet.h"
#if CONFIG_LWIP_NOTHREADS
#include "lwip/timeouts.h"
#else /* CONFIG_LWIP_NOTHREADS */
#include <uk/semaphore.h>
#endif /* CONFIG_LWIP_NOTHREADS */
#include <uk/netdev_core.h>
#include "netif/uknetdev.h"
#include <uk/init.h>
#include <uk/alloc.h>
#include <string.h> /*strcpy*/

#if LWIP_NETIF_EXT_STATUS_CALLBACK && CONFIG_LWIP_NETIF_STATUS_PRINT
#include <stdio.h>

#if CONFIG_LIBFLEXOS_INTELPKU
/* FIXME FLEXOS: do we really need to disable optimizations here? */
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif
static void _netif_status_print(struct netif *nf, netif_nsc_reason_t reason,
				const netif_ext_callback_args_t *args)
{
	char str_ip4_addr[17];
	char str_ip4_mask[17];
	char str_ip4_gw[17];

	if (reason & LWIP_NSC_NETIF_ADDED) {
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Added\n"),
				  nf->name[0], nf->name[1], nf->num);
	}
	if (reason & LWIP_NSC_NETIF_REMOVED) {
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Removed\n"),
				  nf->name[0], nf->name[1], nf->num);
	}
	if (reason & LWIP_NSC_LINK_CHANGED) {
		char *state = args->link_changed.state ? FLEXOS_SHARED_LITERAL("up") : FLEXOS_SHARED_LITERAL("down");
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Link is %s\n"),
				  nf->name[0], nf->name[1], nf->num, state);
	}
	if (reason & LWIP_NSC_STATUS_CHANGED) {
		char *state = args->link_changed.state ? FLEXOS_SHARED_LITERAL("up") : FLEXOS_SHARED_LITERAL("down");
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Interface is %s\n"),
				  nf->name[0], nf->name[1], nf->num, state);
	}

#if LWIP_IPV4
	if ((reason & LWIP_NSC_IPV4_SETTINGS_CHANGED)
	    || (reason & LWIP_NSC_IPV4_ADDRESS_CHANGED)
	    || (reason & LWIP_NSC_IPV4_NETMASK_CHANGED)
	    || (reason & LWIP_NSC_IPV4_GATEWAY_CHANGED)) {
		ipaddr_ntoa_r(&nf->ip_addr, str_ip4_addr, sizeof(str_ip4_addr));
		ipaddr_ntoa_r(&nf->netmask, str_ip4_mask, sizeof(str_ip4_mask));
		ipaddr_ntoa_r(&nf->gw,      str_ip4_gw,   sizeof(str_ip4_gw));

/* Commented as the number of parameters exceeds 6.
		flexos_gate(libc, printf,
		       FLEXOS_SHARED_LITERAL("%c%c%u: Set IPv4 address %s mask %s gw %s\n"),
		       nf->name[0], nf->name[1], nf->num,
		       str_ip4_addr, str_ip4_mask, str_ip4_gw);
*/
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Set IPv4 address %s mask %s\n"),
				  nf->name[0], nf->name[1], nf->num,
				  str_ip4_addr, str_ip4_mask);
	}
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
	if (reason & LWIP_NSC_IPV6_SET) {
/* Commented as the number of parameters exceeds 6.
		flexos_gate(libc, printf,
		       FLEXOS_SHARED_LITERAL("%c%c%u: Set IPv6 address %d: %s (state %d)\n"),
		       nf->name[0], nf->name[1], nf->num,
		       args->ipv6_set.addr_index,
		       ipaddr_ntoa(&nf->ip6_addr[args->ipv6_set.addr_index]),
		       nf->ip6_addr_state[args->ipv6_set.addr_index]);
*/
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Set IPv6 address %d: %s\n"),
				  nf->name[0], nf->name[1], nf->num,
				  args->ipv6_set.addr_index,
				  ipaddr_ntoa(&nf->ip6_addr[args->ipv6_set.addr_index]));
	}
	if (reason & LWIP_NSC_IPV6_ADDR_STATE_CHANGED) {
/* Commented as the number of parameters exceeds 6.
		flexos_gate(libc, printf,
		       FLEXOS_SHARED_LITERAL("%c%c%u: Set IPv6 address %d: %s (state %d)\n"),
		       nf->name[0], nf->name[1], nf->num,
		       args->ipv6_set.addr_index,
		       ipaddr_ntoa(&nf->ip6_addr[
				     args->ipv6_addr_state_changed.addr_index]),
		       nf->ip6_addr_state[
				     args->ipv6_addr_state_changed.addr_index]);
*/
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, printf,
				  FLEXOS_SHARED_LITERAL("%c%c%u: Set IPv6 address %d: %s\n"),
				  nf->name[0], nf->name[1], nf->num,
				  args->ipv6_set.addr_index,
				  ipaddr_ntoa(&nf->ip6_addr[args->ipv6_addr_state_changed.addr_index]));
	}
#endif /* LWIP_IPV6 */
}
#if CONFIG_LIBFLEXOS_INTELPKU
#pragma GCC pop_options
#endif

NETIF_DECLARE_EXT_CALLBACK(netif_status_print)
#endif /* LWIP_NETIF_EXT_STATUS_CALLBACK && CONFIG_LWIP_NETIF_STATUS_PRINT */

void sys_init(void)
{
	/*
	 * This function is called before the any other sys_arch-function is
	 * called and is meant to be used to initialize anything that has to
	 * be up and running for the rest of the functions to work. for
	 * example to set up a pool of semaphores.
	 */
}

#if !CONFIG_LWIP_NOTHREADS
static struct uk_semaphore _lwip_init_sem;

static void _lwip_init_done(void *arg __unused)
{
	flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_semaphore_up, &_lwip_init_sem);
}

#if CONFIG_LIBFLEXOS_VMEPT
/* Local (same-VM) completion for VMEPT — see _liblwip_init. */
static void _lwip_init_done_vmept(void *arg)
{
	struct uk_semaphore *sem = (struct uk_semaphore *)arg;

	if (sem)
		uk_semaphore_up(sem);
}
#endif /* CONFIG_LIBFLEXOS_VMEPT */
#endif /* !CONFIG_LWIP_NOTHREADS */

#ifndef CONFIG_LIBFLEXOS_VMEPT
static
#endif
void uk_netdev_einfo_get_with_copy(struct uk_netdev *dev,
	enum uk_netdev_einfo_type einfo, char **strcfg)
{
	/* .data_shared is visible to both VMs at the same VA; avoid
	 * allocating from Comp0's shared-heap half (Comp1 cannot free it). */
	static char buf[64] __attribute__((section(".data_shared")));
	char *_tmp = uk_netdev_einfo_get(dev, einfo);

	if (!_tmp) {
		*strcfg = NULL;
		return;
	}
	strncpy(buf, _tmp, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	*strcfg = buf;
}

/*
 * This function initializing the lwip network stack
 */
int _liblwip_init(void);
                                                                       __attribute__((section(".text_shared")))                                                                       
__attribute__((libukboot_callback))
int liblwip_init(void)
{
	int ret;
	flexos_nop_gate_r(0, 0, ret, _liblwip_init);
	return ret;
}

int _liblwip_init(void)
{
#if CONFIG_LWIP_UKNETDEV && CONFIG_LWIP_AUTOIFACE
	unsigned int devid;
	struct uk_netdev *dev;
	struct netif *nf;
	const char  __maybe_unused **strcfg = uk_malloc(flexos_shared_alloc, sizeof(char *));
	*strcfg = NULL;
	uint16_t  __maybe_unused int16cfg;
	int is_first_nf;
#if LWIP_IPV4
	ip4_addr_t ip4;
	ip4_addr_t *ip4_arg;
	ip4_addr_t mask4;
	ip4_addr_t *mask4_arg;
	ip4_addr_t gw4;
	ip4_addr_t *gw4_arg;
#endif /* LWIP_IPV4 */
#endif /* CONFIG_LWIP_UKNETDEV && CONFIG_LWIP_AUTOIFACE */

	flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_info,
			  FLEXOS_SHARED_LITERAL("Initializing lwip\n"));
#if CONFIG_LIBFLEXOS_VMEPT
	/*
	 * VMEPT: synchronize tcpip bring-up with a *local* semaphore.
	 * Cross-VM gate(uk_semaphore_down) deadlocks: the app holds the
	 * nested RPC while waiting on the sem, so the tcpip thread's
	 * gate(uk_semaphore_up) cannot complete on the same ctrl path.
	 */
	{
		static struct uk_semaphore vmept_lwip_sem;
		uk_semaphore_init(&vmept_lwip_sem, 0);
		tcpip_init(_lwip_init_done_vmept, &vmept_lwip_sem);
		uk_semaphore_down(&vmept_lwip_sem);
	}
#elif CONFIG_LWIP_NOTHREADS
	lwip_init();
#else /* CONFIG_LWIP_NOTHREADS */
	flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_semaphore_init, &_lwip_init_sem, 0);
	tcpip_init(_lwip_init_done, NULL);

	/* Wait until stack is booted */
	flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_semaphore_down, &_lwip_init_sem);
#endif /* CONFIG_LIBFLEXOS_VMEPT / CONFIG_LWIP_NOTHREADS */

#if LWIP_NETIF_EXT_STATUS_CALLBACK && CONFIG_LWIP_NETIF_STATUS_PRINT
	/* Add print callback for netif state changes */
	netif_add_ext_callback(&netif_status_print, _netif_status_print);
#endif /* LWIP_NETIF_EXT_STATUS_CALLBACK && CONFIG_LWIP_NETIF_STATUS_PRINT */

#if CONFIG_LWIP_UKNETDEV && CONFIG_LWIP_AUTOIFACE
	is_first_nf = 1;
	unsigned int netdev_count;

	flexos_vmept_gate_r(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, netdev_count, uk_netdev_count);

	for (devid = 0; devid < netdev_count; ++devid) {
		enum uk_netdev_state netdev_state;
		flexos_vmept_gate_r(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, dev, uk_netdev_get, devid);
		if (!dev)
			continue;

		flexos_vmept_gate_r(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, netdev_state, uk_netdev_state_get,
				    dev);

		if (netdev_state != UK_NETDEV_UNCONFIGURED) {
			flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_info,
					  FLEXOS_SHARED_LITERAL("Skipping to add network device %u to lwIP: Not in unconfigured state\n"),
					  devid);
			continue;
		}

		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_info,
				  FLEXOS_SHARED_LITERAL("Attach network device %u to lwIP...\n"),
				  devid);

#if LWIP_IPV4
		ip4_arg   = NULL;
		mask4_arg = NULL;
		gw4_arg   = NULL;

		/* IP */
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_netdev_einfo_get_with_copy, dev,
				  UK_NETDEV_IPV4_ADDR_STR, strcfg);

		if (*strcfg) {
			if (ip4addr_aton(*strcfg, &ip4) != 1) {
				flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_err,
						  FLEXOS_SHARED_LITERAL("Error converting IP address: %s\n"),
						  *strcfg);
				goto no_conf;
			}
			ip4_arg = &ip4;
		} else {
#if CONFIG_LIBFLEXOS_VMEPT
			/* QEMU user-net defaults when libparam did not stick */
			ip4addr_aton("10.0.2.15", &ip4);
			ip4_arg = &ip4;
#else
			goto no_conf;
#endif
		}

		/* mask */
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_netdev_einfo_get_with_copy, dev,
				  UK_NETDEV_IPV4_MASK_STR, strcfg);

		if (*strcfg) {
			if (ip4addr_aton(*strcfg, &mask4) != 1) {
				flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_err,
						  FLEXOS_SHARED_LITERAL("Error converting net mask: %s\n"),
						  *strcfg);
				goto no_conf;
			}
		} else {
#if CONFIG_LIBFLEXOS_VMEPT
			ip4addr_aton("255.255.255.0", &mask4);
#else
			/* default mask */
			ip4_addr_set_u32(&mask4, lwip_htonl(IP_CLASSC_NET));
#endif
		}
		mask4_arg = &mask4;

		/* gateway */
		flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_netdev_einfo_get_with_copy, dev,
				  UK_NETDEV_IPV4_GW_STR, strcfg);

		if (*strcfg) {
			if (ip4addr_aton(*strcfg, &gw4) != 1) {
				flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_err,
						  FLEXOS_SHARED_LITERAL("Error converting gateway: %s\n"),
						  *strcfg);
				goto no_conf;
			}
			gw4_arg = &gw4;
		} else {
#if CONFIG_LIBFLEXOS_VMEPT
			ip4addr_aton("10.0.2.2", &gw4);
			gw4_arg = &gw4;
#endif
		}
no_conf:
		nf = uknetdev_addif(dev, ip4_arg, mask4_arg, gw4_arg);
#else /* LWIP_IPV4 */
		/*
		 * TODO: Add support for IPv6 device configuration from
		 * netdev's econf interface
		 */

		nf = uknetdev_addif(dev);
#endif /* LWIP_IPV4 */
		if (!nf) {
			flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_err,
					  FLEXOS_SHARED_LITERAL("Failed to attach network device %u to lwIP\n"),
					  devid);
			continue;
		}

		/* Declare the first network device as default interface */
		if (is_first_nf) {
			flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_info,
					  FLEXOS_SHARED_LITERAL("%c%c%u: Set as default interface\n"),
					  nf->name[0], nf->name[1], nf->num);
			netif_set_default(nf);
			is_first_nf = 0;
		}
		netif_set_up(nf);

#if LWIP_IPV4 && LWIP_DHCP
		if (!ip4_arg) {
			flexos_vmept_gate(flexos_vmept_comp_id, FLEXOS_VMEPT_APPCOMP, uk_pr_info,
					  FLEXOS_SHARED_LITERAL("%c%c%u: DHCP configuration (background)...\n"),
					  nf->name[0], nf->name[1], nf->num);
			dhcp_start(nf);
		}
#endif /* LWIP_IPV4 && LWIP_DHCP */
	}
#endif /* CONFIG_LWIP_UKNETDEV && CONFIG_LWIP_AUTOIFACE */
#if CONFIG_LWIP_UKNETDEV && CONFIG_LWIP_AUTOIFACE
	uk_free(flexos_shared_alloc, strcfg);
#endif /* CONFIG_LWIP_UKNETDEV && CONFIG_LWIP_AUTOIFACE */
	return 0;
}
/* FOLKS! THIS IS BAD! TODO FIXME FLEXOS! This was done for the
 * ASPLOS deadline, because we don't have time to ensure that initcalls
 * land into the right binary. Adress this later. */
#if !CONFIG_LIBFLEXOS_VMEPT
/* fabric-isolated: liblwip_init invoked via fabric gate */
#elif defined(FLEXOS_VMEPT_COMP_COUNT) && (FLEXOS_VMEPT_COMP_COUNT <= 1)
/*
 * Monolith VMEPT (n01): no peer compartment and auto_register does not
 * emit __fabric_isolated_lwip_init (lwip stays on the default fabric).
 * Restore a local initcall so memp/tcpip come up before nginx_main.
 */
/* fabric-isolated: liblwip_init invoked via fabric gate */
#endif
