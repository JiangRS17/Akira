// Auto-generated for /root/.unikraft_akira/unikraft/lib/uknetdev/netdev.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_sched_thread_create, EL)
+ fabric_gate(0, uk_sched_thread_create, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_sched_thread_create, EL)
+ fabric_gate_r(0, RET, uk_sched_thread_create, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_thread_wait, EL)
+ fabric_gate(0, uk_thread_wait, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_thread_wait, EL)
+ fabric_gate_r(0, RET, uk_thread_wait, EL)

