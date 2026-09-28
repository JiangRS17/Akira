// Auto-generated for /root/.unikraft_akira/unikraft/lib/example2/example2.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, example3_empty, EL)
+ fabric_gate(1, example3_empty, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, example3_empty, EL)
+ fabric_gate_r(1, RET, example3_empty, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, e3c1, EL)
+ fabric_gate(1, e3c1, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, e3c1, EL)
+ fabric_gate_r(1, RET, e3c1, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, e3c2, EL)
+ fabric_gate(1, e3c2, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, e3c2, EL)
+ fabric_gate_r(1, RET, e3c2, EL)

