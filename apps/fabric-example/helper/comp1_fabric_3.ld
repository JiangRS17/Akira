.data_comp1_fabric_3 : {
    *(.data_comp1_fabric_3 .data_comp1_fabric_3.*)
}
INSERT AFTER .data;

.bss_comp1_fabric_3 : {
    *(.bss_comp1_fabric_3 .bss_comp1_fabric_3.*)
}
INSERT AFTER .bss;

.text_comp1_fabric_3 : {
    *(.text_comp1_fabric_3 .text_comp1_fabric_3.*)
}
INSERT AFTER .text;

_fabric_comp1_fabric_3_data_start = ADDR(.data_comp1_fabric_3);
_fabric_comp1_fabric_3_data_end   = ADDR(.data_comp1_fabric_3) + SIZEOF(.data_comp1_fabric_3);
_fabric_comp1_fabric_3_bss_start  = ADDR(.bss_comp1_fabric_3);
_fabric_comp1_fabric_3_bss_end    = ADDR(.bss_comp1_fabric_3) + SIZEOF(.bss_comp1_fabric_3);
_fabric_comp1_fabric_3_text_start = ADDR(.text_comp1_fabric_3);
_fabric_comp1_fabric_3_text_end   = ADDR(.text_comp1_fabric_3) + SIZEOF(.text_comp1_fabric_3);
