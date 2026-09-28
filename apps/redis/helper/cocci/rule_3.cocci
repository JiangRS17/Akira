// Auto-generated for /root/.unikraft_akira/apps/redis/build/libredis/origin/redis-5.0.6/src/anet.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, accept, EL)
+ fabric_gate(0, accept, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, accept, EL)
+ fabric_gate_r(0, RET, accept, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, bind, EL)
+ fabric_gate(0, bind, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, bind, EL)
+ fabric_gate_r(0, RET, bind, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, freeaddrinfo, EL)
+ fabric_gate(0, freeaddrinfo, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, freeaddrinfo, EL)
+ fabric_gate_r(0, RET, freeaddrinfo, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, getaddrinfo, EL)
+ fabric_gate(0, getaddrinfo, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, getaddrinfo, EL)
+ fabric_gate_r(0, RET, getaddrinfo, EL)

@noreturn_4@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, getpeername, EL)
+ fabric_gate(0, getpeername, EL)

@return_4@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, getpeername, EL)
+ fabric_gate_r(0, RET, getpeername, EL)

@noreturn_5@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, listen, EL)
+ fabric_gate(0, listen, EL)

@return_5@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, listen, EL)
+ fabric_gate_r(0, RET, listen, EL)

@noreturn_6@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, setsockopt, EL)
+ fabric_gate(0, setsockopt, EL)

@return_6@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, setsockopt, EL)
+ fabric_gate_r(0, RET, setsockopt, EL)

@noreturn_7@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, socket, EL)
+ fabric_gate(0, socket, EL)

@return_7@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, socket, EL)
+ fabric_gate_r(0, RET, socket, EL)

