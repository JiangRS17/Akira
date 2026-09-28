/* 最终内核链接（link64.lds）用：收集已重命名好的 comp1_fabric-3 段并导出 MPK 边界符号。
 * 必须使用 INSERT AFTER 片段格式，不可使用独立 SECTIONS { } 块覆盖默认 .text/.data/.bss。 */

SECTIONS
{
	.data_comp1_fabric_3 : {
		*(.data_comp1_fabric_3 .data_comp1_fabric_3.*)
	}
}
INSERT AFTER .data;

SECTIONS
{
	.bss_comp1_fabric_3 : {
		*(.bss_comp1_fabric_3 .bss_comp1_fabric_3.*)
	}
}
INSERT AFTER .bss;

SECTIONS
{
	.text_comp1_fabric_3 : {
		*(.text_comp1_fabric_3 .text_comp1_fabric_3.*)
	}
}
INSERT AFTER .text;

_fabric_comp1_fabric_3_data_start = ADDR(.data_comp1_fabric_3);
_fabric_comp1_fabric_3_data_end   = ADDR(.data_comp1_fabric_3) + SIZEOF(.data_comp1_fabric_3);
_fabric_comp1_fabric_3_bss_start  = ADDR(.bss_comp1_fabric_3);
_fabric_comp1_fabric_3_bss_end    = ADDR(.bss_comp1_fabric_3) + SIZEOF(.bss_comp1_fabric_3);
_fabric_comp1_fabric_3_text_start = ADDR(.text_comp1_fabric_3);
_fabric_comp1_fabric_3_text_end   = ADDR(.text_comp1_fabric_3) + SIZEOF(.text_comp1_fabric_3);

/*
 * ---- 旧写法（已废弃，保留备查）----
 * 问题：同 comp1_fabric_2.ld，独立 SECTIONS { } 块不适合 partial link。
 *
 * SECTIONS {
 *     .data_comp1_fabric_3 : {
 *         *(.data_comp1_fabric_3 .data_comp1_fabric_3.*)
 *     }
 *     .bss_comp1_fabric_3 : {
 *         *(.bss_comp1_fabric_3 .bss_comp1_fabric_3.*)
 *     }
 *     .text_comp1_fabric_3 : {
 *         *(.text_comp1_fabric_3 .text_comp1_fabric_3.*)
 *     }
 * }
 *
 * _fabric_comp1_fabric_3_data_start = ADDR(.data_comp1_fabric_3);
 * _fabric_comp1_fabric_3_data_end   = ADDR(.data_comp1_fabric_3) + SIZEOF(.data_comp1_fabric_3);
 * _fabric_comp1_fabric_3_bss_start  = ADDR(.bss_comp1_fabric_3);
 * _fabric_comp1_fabric_3_bss_end    = ADDR(.bss_comp1_fabric_3) + SIZEOF(.bss_comp1_fabric_3);
 * _fabric_comp1_fabric_3_text_start = ADDR(.text_comp1_fabric_3);
 * _fabric_comp1_fabric_3_text_end   = ADDR(.text_comp1_fabric_3) + SIZEOF(.text_comp1_fabric_3);
 */
