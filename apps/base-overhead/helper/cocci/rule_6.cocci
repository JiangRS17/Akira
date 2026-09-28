// Auto-generated for /root/.unikraft_akira/unikraft/lib/example4/example4.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, example5_empty, EL)
+ fabric_gate(3, example5_empty, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, example5_empty, EL)
+ fabric_gate_r(3, RET, example5_empty, EL)

