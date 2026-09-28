// Auto-generated for /root/.unikraft_akira/libs/lwip/sockets.c

@noreturn_0@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, dentry_alloc, EL)
+ fabric_gate(1, dentry_alloc, EL)

@return_0@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, dentry_alloc, EL)
+ fabric_gate_r(1, RET, dentry_alloc, EL)

@noreturn_1@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, drele, EL)
+ fabric_gate(1, drele, EL)

@return_1@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, drele, EL)
+ fabric_gate_r(1, RET, drele, EL)

@noreturn_2@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, vfscore_alloc_fd, EL)
+ fabric_gate(1, vfscore_alloc_fd, EL)

@return_2@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, vfscore_alloc_fd, EL)
+ fabric_gate_r(1, RET, vfscore_alloc_fd, EL)

@noreturn_3@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, vfscore_install_fd, EL)
+ fabric_gate(1, vfscore_install_fd, EL)

@return_3@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, vfscore_install_fd, EL)
+ fabric_gate_r(1, RET, vfscore_install_fd, EL)

@noreturn_4@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, vfscore_put_fd, EL)
+ fabric_gate(1, vfscore_put_fd, EL)

@return_4@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, vfscore_put_fd, EL)
+ fabric_gate_r(1, RET, vfscore_put_fd, EL)

@noreturn_5@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, vfscore_put_file, EL)
+ fabric_gate(1, vfscore_put_file, EL)

@return_5@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, vfscore_put_file, EL)
+ fabric_gate_r(1, RET, vfscore_put_file, EL)

@noreturn_6@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, vfscore_vget, EL)
+ fabric_gate(1, vfscore_vget, EL)

@return_6@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, vfscore_vget, EL)
+ fabric_gate_r(1, RET, vfscore_vget, EL)

@noreturn_7@
expression list EL;
expression COMP_FROM, COMP_TO;
@@
- flexos_nop_gate(COMP_FROM, COMP_TO, vrele, EL)
+ fabric_gate(1, vrele, EL)

@return_7@
expression list EL;
expression COMP_FROM, COMP_TO, RET;
@@
- flexos_nop_gate_r(COMP_FROM, COMP_TO, RET, vrele, EL)
+ fabric_gate_r(1, RET, vrele, EL)

