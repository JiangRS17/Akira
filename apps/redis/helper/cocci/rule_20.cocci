// Auto-generated for /root/.unikraft_akira/unikraft/lib/uktime/time.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_sched_thread_sleep, EL)
+ fabric_gate(0, uk_sched_thread_sleep, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_sched_thread_sleep, EL)
+ fabric_gate_r(0, RET, uk_sched_thread_sleep, EL)

