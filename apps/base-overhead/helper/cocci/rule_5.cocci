// Auto-generated for /root/.unikraft_akira/unikraft/lib/example3/example3.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, example4_empty, EL)
+ fabric_gate(2, example4_empty, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, example4_empty, EL)
+ fabric_gate_r(2, RET, example4_empty, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, e4c1, EL)
+ fabric_gate(2, e4c1, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, e4c1, EL)
+ fabric_gate_r(2, RET, e4c1, EL)

