// Auto-generated for /root/.unikraft_akira/apps/redis/build/liblwip/origin/lwip-2.1.2/src/api/netdb.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, atoi, EL)
+ fabric_gate(1, atoi, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, atoi, EL)
+ fabric_gate_r(1, RET, atoi, EL)

