// Auto-generated for /root/.unikraft_akira/libs/lwip/netbuf.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netbuf_free_single, EL)
+ fabric_gate(1, uk_netbuf_free_single, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netbuf_free_single, EL)
+ fabric_gate_r(1, RET, uk_netbuf_free_single, EL)

