// Auto-generated for /root/.unikraft_akira/unikraft/lib/uk9p/9preq.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_thread_wake, EL)
+ fabric_gate(0, uk_thread_wake, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_thread_wake, EL)
+ fabric_gate_r(0, RET, uk_thread_wake, EL)

