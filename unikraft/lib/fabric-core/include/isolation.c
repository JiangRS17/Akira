#include "../fabric/include/fabric.h"
#include "./fabric_error.h"
#include "./fabric/isolation.h"
#include "../message/include/message.h"
#include "../controller/include/controller.h"
#include "../overhead/include/overhead.h"
#include <uk/print.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

void fabric_start(uint32_t src_fabric_id, struct FabricMsg *msg)
{
	struct Fabric *fabric;
	fabric = g_fabrics[src_fabric_id];
	if (!fabric){
		uk_pr_err("[fabric-gate-error] fabric is NULL (id=%d)\n", src_fabric_id);
		if (msg)
			msg->done = 1;
		return;
	}
	if(fabric_send_message(fabric, msg) != FABRIC_SUCCESS){
		uk_pr_err("[fabric-gate-error] failed to send message\n");
		if (msg)
			msg->done = 1;
		return;
	}
}

void _fabric_do_gate(uint32_t src_fabric_id,
		     const char *name, void *direct_fn, void *ret_ptr,
		     size_t ret_size, int has_ret, int argc, ...)
{
	va_list ap;
	int i;
	uint64_t t0 = 0;
	int prof = fabric_overhead_enabled();

	if (!name || !direct_fn || argc < 0 || argc > 11) {
		uk_pr_err("[fabric-gate-error] invalid gate metadata argc=%d\n", argc);
		return;
	}

	if (prof)
		t0 = fabric_ov_tsc();

	/* 栈分配 FabricMsg */
	struct FabricMsg msg_stack;
	msg_stack.src_fabric_id = src_fabric_id;
	msg_stack.service_id    = fabric_service_id_from_name(name);
	msg_stack.func_name     = name;
	msg_stack.func_ptr      = direct_fn;
	msg_stack.has_ret       = has_ret;
	msg_stack.ret_ptr       = ret_ptr;
	msg_stack.ret_size      = ret_size;
	msg_stack.dst_fabric_id = (uint32_t)-1;
	msg_stack.done          = 0;
	msg_stack.priority      = 0;
	msg_stack.argc          = argc;

	va_start(ap, argc);
	for (i = 0; i < argc; i++)
		msg_stack.args[i] = va_arg(ap, long);
	va_end(ap);

	if (prof) {
		uint64_t t1 = fabric_ov_tsc();
		struct fabric_overhead_stats *st = fabric_overhead_stats_ptr();

		st->gate_build += t1 - t0;
	}

	fabric_start(src_fabric_id, &msg_stack);

	if (!msg_stack.done)
		uk_pr_err("[fabric-gate-error] dispatch not complete: func_ptr=%p\n", direct_fn);
}

void _fabric_do_gate_dst(uint32_t src_fabric_id, uint32_t dst_fabric_id,
			 const char *name, void *direct_fn, void *ret_ptr,
			 size_t ret_size, int has_ret, int argc, ...)
{
	va_list ap;
	int i;
	struct FabricMsg msg_stack;

	if (!name || !direct_fn || argc < 0 || argc > 11)
		return;

	msg_stack.src_fabric_id = src_fabric_id;
	msg_stack.service_id    = fabric_service_id_from_name(name);
	msg_stack.func_name     = name;
	msg_stack.func_ptr      = direct_fn;
	msg_stack.has_ret       = has_ret;
	msg_stack.ret_ptr       = ret_ptr;
	msg_stack.ret_size      = ret_size;
	msg_stack.dst_fabric_id = dst_fabric_id;
	msg_stack.done          = 0;
	msg_stack.priority      = 0;
	msg_stack.argc          = argc;

	va_start(ap, argc);
	for (i = 0; i < argc; i++)
		msg_stack.args[i] = va_arg(ap, long);
	va_end(ap);

	fabric_start(src_fabric_id, &msg_stack);
}
