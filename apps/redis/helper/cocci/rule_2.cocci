// Auto-generated for /root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/ae_select.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, select, EL)
+ fabric_gate(0, select, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, select, EL)
+ fabric_gate_r(0, RET, select, EL)

