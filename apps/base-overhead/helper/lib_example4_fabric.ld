SECTIONS {
    .data_comp1_fabric_4 : {
        *(.data .data.* .rodata .rodata.* .sdata .sdata.*)
    }
    .bss_comp1_fabric_4 : {
        *(.bss .bss.* .sbss .sbss.* COMMON)
    }
    .text_comp1_fabric_4 : {
        *(.text .text.*)
    }
}
