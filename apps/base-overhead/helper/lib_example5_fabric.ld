SECTIONS {
    .data_comp1_fabric_5 : {
        *(.data .data.* .rodata .rodata.* .sdata .sdata.*)
    }
    .bss_comp1_fabric_5 : {
        *(.bss .bss.* .sbss .sbss.* COMMON)
    }
    .text_comp1_fabric_5 : {
        *(.text .text.*)
    }
}
