// Auto-generated for /root/.unikraft_akira/libs/lwip/mailbox.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_create, EL)
+ fabric_gate(1, uk_mbox_create, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_create, EL)
+ fabric_gate_r(1, RET, uk_mbox_create, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_free, EL)
+ fabric_gate(1, uk_mbox_free, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_free, EL)
+ fabric_gate_r(1, RET, uk_mbox_free, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_post, EL)
+ fabric_gate(1, uk_mbox_post, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_post, EL)
+ fabric_gate_r(1, RET, uk_mbox_post, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_post_try, EL)
+ fabric_gate(1, uk_mbox_post_try, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_post_try, EL)
+ fabric_gate_r(1, RET, uk_mbox_post_try, EL)

@noreturn_4@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_recv, EL)
+ fabric_gate(1, uk_mbox_recv, EL)

@return_4@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_recv, EL)
+ fabric_gate_r(1, RET, uk_mbox_recv, EL)

@noreturn_5@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_recv_to, EL)
+ fabric_gate(1, uk_mbox_recv_to, EL)

@return_5@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_recv_to, EL)
+ fabric_gate_r(1, RET, uk_mbox_recv_to, EL)

@noreturn_6@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, uk_mbox_recv_try, EL)
+ fabric_gate(1, uk_mbox_recv_try, EL)

@return_6@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, uk_mbox_recv_try, EL)
+ fabric_gate_r(1, RET, uk_mbox_recv_try, EL)

