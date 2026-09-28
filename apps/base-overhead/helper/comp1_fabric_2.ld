/* 最终内核链接（link64.lds）用：收集已重命名好的 comp1_fabric-2 段并导出 MPK 边界符号。
 * 必须使用 INSERT AFTER 片段格式，不可使用独立 SECTIONS { } 块覆盖默认 .text/.data/.bss。 */

SECTIONS
{
	.data_comp1_fabric_2 : {
		*(.data_comp1_fabric_2 .data_comp1_fabric_2.*)
	}
}
INSERT AFTER .data;

SECTIONS
{
	.bss_comp1_fabric_2 : {
		*(.bss_comp1_fabric_2 .bss_comp1_fabric_2.*)
	}
}
INSERT AFTER .bss;

SECTIONS
{
	.text_comp1_fabric_2 : {
		*(.text_comp1_fabric_2 .text_comp1_fabric_2.*)
	}
}
INSERT AFTER .text;

_fabric_comp1_fabric_2_data_start = ADDR(.data_comp1_fabric_2);
_fabric_comp1_fabric_2_data_end   = ADDR(.data_comp1_fabric_2) + SIZEOF(.data_comp1_fabric_2);
_fabric_comp1_fabric_2_bss_start  = ADDR(.bss_comp1_fabric_2);
_fabric_comp1_fabric_2_bss_end    = ADDR(.bss_comp1_fabric_2) + SIZEOF(.bss_comp1_fabric_2);
_fabric_comp1_fabric_2_text_start = ADDR(.text_comp1_fabric_2);
_fabric_comp1_fabric_2_text_end   = ADDR(.text_comp1_fabric_2) + SIZEOF(.text_comp1_fabric_2);

/*
 * ---- 旧写法（已废弃，保留备查）----
 * 问题：独立 SECTIONS { } 块在库级 partial link 时会覆盖默认脚本，
 *       导致 flexos_extra.ld 报 ".text not found for insert"。
 *       此脚本仅应用于最终 link64.lds 注入，不应出现在 LIBEXAMPLE2_LDFLAGS 中。
 *
 * SECTIONS {
 *     .data_comp1_fabric_2 : {
 *         *(.data_comp1_fabric_2 .data_comp1_fabric_2.*)
 *     }
 *     .bss_comp1_fabric_2 : {
 *         *(.bss_comp1_fabric_2 .bss_comp1_fabric_2.*)
 *     }
 *     .text_comp1_fabric_2 : {
 *         *(.text_comp1_fabric_2 .text_comp1_fabric_2.*)
 *     }
 * }
 *
 * _fabric_comp1_fabric_2_data_start = ADDR(.data_comp1_fabric_2);
 * _fabric_comp1_fabric_2_data_end   = ADDR(.data_comp1_fabric_2) + SIZEOF(.data_comp1_fabric_2);
 * _fabric_comp1_fabric_2_bss_start  = ADDR(.bss_comp1_fabric_2);
 * _fabric_comp1_fabric_2_bss_end    = ADDR(.bss_comp1_fabric_2) + SIZEOF(.bss_comp1_fabric_2);
 * _fabric_comp1_fabric_2_text_start = ADDR(.text_comp1_fabric_2);
 * _fabric_comp1_fabric_2_text_end   = ADDR(.text_comp1_fabric_2) + SIZEOF(.text_comp1_fabric_2);
 */
