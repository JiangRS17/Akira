SECTIONS {
    .data_comp1_fabric_3 : {
        *(.data .data.* .rodata .rodata.* .sdata .sdata.*)
    }
    .bss_comp1_fabric_3 : {
        *(.bss .bss.* .sbss .sbss.* COMMON)
    }
    .text_comp1_fabric_3 : {
        *(.text .text.*)
    }
}
