SECTIONS {
    .data_comp1_fabric_2 : {
        *(.data .data.* .rodata .rodata.* .sdata .sdata.*)
    }
    .bss_comp1_fabric_2 : {
        *(.bss .bss.* .sbss .sbss.* COMMON)
    }
    .text_comp1_fabric_2 : {
        *(.text .text.*)
    }
}
