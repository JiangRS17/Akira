// Auto-generated for /root/.unikraft_akira/libs/lwip/uknetdev.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_configure, EL)
+ fabric_gate(1, uk_netdev_configure, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_configure, EL)
+ fabric_gate_r(1, RET, uk_netdev_configure, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_hwaddr_get, EL)
+ fabric_gate(1, uk_netdev_hwaddr_get, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_hwaddr_get, EL)
+ fabric_gate_r(1, RET, uk_netdev_hwaddr_get, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_info_get, EL)
+ fabric_gate(1, uk_netdev_info_get, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_info_get, EL)
+ fabric_gate_r(1, RET, uk_netdev_info_get, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_mtu_get, EL)
+ fabric_gate(1, uk_netdev_mtu_get, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_mtu_get, EL)
+ fabric_gate_r(1, RET, uk_netdev_mtu_get, EL)

@noreturn_4@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_rxq_configure, EL)
+ fabric_gate(1, uk_netdev_rxq_configure, EL)

@return_4@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_rxq_configure, EL)
+ fabric_gate_r(1, RET, uk_netdev_rxq_configure, EL)

@noreturn_5@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_rxq_intr_enable, EL)
+ fabric_gate(1, uk_netdev_rxq_intr_enable, EL)

@return_5@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_rxq_intr_enable, EL)
+ fabric_gate_r(1, RET, uk_netdev_rxq_intr_enable, EL)

@noreturn_6@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_start, EL)
+ fabric_gate(1, uk_netdev_start, EL)

@return_6@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_start, EL)
+ fabric_gate_r(1, RET, uk_netdev_start, EL)

@noreturn_7@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_state_get, EL)
+ fabric_gate(1, uk_netdev_state_get, EL)

@return_7@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_state_get, EL)
+ fabric_gate_r(1, RET, uk_netdev_state_get, EL)

@noreturn_8@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_netdev_txq_configure, EL)
+ fabric_gate(1, uk_netdev_txq_configure, EL)

@return_8@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_netdev_txq_configure, EL)
+ fabric_gate_r(1, RET, uk_netdev_txq_configure, EL)

