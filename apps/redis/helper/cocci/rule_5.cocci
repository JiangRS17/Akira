// Auto-generated for /root/.unikraft_akira/libs/lwip/init.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, printf, EL)
+ fabric_gate(1, printf, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, printf, EL)
+ fabric_gate_r(1, RET, printf, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_count, EL)
+ fabric_gate(1, uk_netdev_count, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_count, EL)
+ fabric_gate_r(1, RET, uk_netdev_count, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_get, EL)
+ fabric_gate(1, uk_netdev_get, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_get, EL)
+ fabric_gate_r(1, RET, uk_netdev_get, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_state_get, EL)
+ fabric_gate(1, uk_netdev_state_get, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_state_get, EL)
+ fabric_gate_r(1, RET, uk_netdev_state_get, EL)

@noreturn_4@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_semaphore_init, EL)
+ fabric_gate(1, uk_semaphore_init, EL)

@return_4@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_semaphore_init, EL)
+ fabric_gate_r(1, RET, uk_semaphore_init, EL)

