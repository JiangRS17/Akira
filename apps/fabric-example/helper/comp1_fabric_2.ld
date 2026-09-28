.data_comp1_fabric_2 : {
    *(.data_comp1_fabric_2 .data_comp1_fabric_2.*)
}
INSERT AFTER .data;

.bss_comp1_fabric_2 : {
    *(.bss_comp1_fabric_2 .bss_comp1_fabric_2.*)
}
INSERT AFTER .bss;

.text_comp1_fabric_2 : {
    *(.text_comp1_fabric_2 .text_comp1_fabric_2.*)
}
INSERT AFTER .text;

_fabric_comp1_fabric_2_data_start = ADDR(.data_comp1_fabric_2);
_fabric_comp1_fabric_2_data_end   = ADDR(.data_comp1_fabric_2) + SIZEOF(.data_comp1_fabric_2);
_fabric_comp1_fabric_2_bss_start  = ADDR(.bss_comp1_fabric_2);
_fabric_comp1_fabric_2_bss_end    = ADDR(.bss_comp1_fabric_2) + SIZEOF(.bss_comp1_fabric_2);
_fabric_comp1_fabric_2_text_start = ADDR(.text_comp1_fabric_2);
_fabric_comp1_fabric_2_text_end   = ADDR(.text_comp1_fabric_2) + SIZEOF(.text_comp1_fabric_2);
