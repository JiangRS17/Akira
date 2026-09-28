// Auto-generated for /root/.unikraft_akira/libs/pthread-embedded/pte_osal.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_sched_thread_exit, EL)
+ fabric_gate(0, uk_sched_thread_exit, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_sched_thread_exit, EL)
+ fabric_gate_r(0, RET, uk_sched_thread_exit, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_sched_thread_sleep, EL)
+ fabric_gate(0, uk_sched_thread_sleep, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_sched_thread_sleep, EL)
+ fabric_gate_r(0, RET, uk_sched_thread_sleep, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_thread_get_prio, EL)
+ fabric_gate(0, uk_thread_get_prio, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_thread_get_prio, EL)
+ fabric_gate_r(0, RET, uk_thread_get_prio, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_thread_inherit_signal_mask, EL)
+ fabric_gate(0, uk_thread_inherit_signal_mask, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_thread_inherit_signal_mask, EL)
+ fabric_gate_r(0, RET, uk_thread_inherit_signal_mask, EL)

@noreturn_4@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_thread_set_prio, EL)
+ fabric_gate(0, uk_thread_set_prio, EL)

@return_4@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_thread_set_prio, EL)
+ fabric_gate_r(0, RET, uk_thread_set_prio, EL)

@noreturn_5@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_thread_wait, EL)
+ fabric_gate(0, uk_thread_wait, EL)

@return_5@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_thread_wait, EL)
+ fabric_gate_r(0, RET, uk_thread_wait, EL)

