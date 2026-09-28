// Auto-generated for /root/.unikraft_akira/apps/base-overhead/cross_domain.c

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

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, e2c1, EL)
+ fabric_gate(0, e2c1, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, e2c1, EL)
+ fabric_gate_r(0, RET, e2c1, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, e2c2, EL)
+ fabric_gate(0, e2c2, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, e2c2, EL)
+ fabric_gate_r(0, RET, e2c2, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, e2c3, EL)
+ fabric_gate(0, e2c3, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, e2c3, EL)
+ fabric_gate_r(0, RET, e2c3, EL)

