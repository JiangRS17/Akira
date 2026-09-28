// Auto-generated for /root/.unikraft_akira/unikraft/lib/uksched/sched.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, flexos_intelpku_mem_set_key, EL)
+ fabric_gate(1, flexos_intelpku_mem_set_key, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, flexos_intelpku_mem_set_key, EL)
+ fabric_gate_r(1, RET, flexos_intelpku_mem_set_key, EL)

