// Auto-generated for /root/.unikraft_akira/apps/base-overhead/cross_server1.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, example2_empty, EL)
+ fabric_gate(0, example2_empty, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, example2_empty, EL)
+ fabric_gate_r(0, RET, example2_empty, EL)

