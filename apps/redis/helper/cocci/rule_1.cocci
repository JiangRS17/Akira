// Auto-generated for /root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/deps/hiredis/net.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, getaddrinfo, EL)
+ fabric_gate(0, getaddrinfo, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, getaddrinfo, EL)
+ fabric_gate_r(0, RET, getaddrinfo, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, setsockopt, EL)
+ fabric_gate(0, setsockopt, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, setsockopt, EL)
+ fabric_gate_r(0, RET, setsockopt, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, socket, EL)
+ fabric_gate(0, socket, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, socket, EL)
+ fabric_gate_r(0, RET, socket, EL)

