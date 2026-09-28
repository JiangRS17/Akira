SECTIONS {
    .data_comp1_fabric_1 : {
        *(.data .data.* .rodata .rodata.* .sdata .sdata.*)
    }
    .bss_comp1_fabric_1 : {
        *(.bss .bss.* .sbss .sbss.* COMMON)
    }
    .text_comp1_fabric_1 : {
        *(.text .text.*)
    }
}
