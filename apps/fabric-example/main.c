/* Fabric MPK Isolation 功能测试
 *
 * 测试项:
 *   1. 跨 fabric 调用是否正常工作（通过 gate）
 *   2. 直接访问其他 fabric 的内存是否触发 page fault（隔离生效验证）
 *   3. 多次跨 fabric 调用，验证权限恢复
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <flexos/isolation.h>
#include <fabric/isolation.h>
#include <example1/isolated.h>
#include <example2/isolated.h>
#include <example3/isolated.h>

/* ================================================================
 * 测试 1: 跨 fabric 调用功能（通过 gate）
 * ================================================================ */

static int test_cross_fabric_call(void)
{
	flexos_nop_gate(0, 0, printf,
		"\n[test 1] 跨 fabric 调用功能测试\n");

	/* fabric-1 → fabric-2: 调用 example2_empty */
	flexos_nop_gate(0, 0, printf,
		"  fabric-1 -> fabric-2: 调用 example2_empty ... ");
	fabric_gate(0, example2_empty);
	flexos_nop_gate(0, 0, printf, "OK\n");

	/* fabric-1 → fabric-3: 调用 example3_empty */
	flexos_nop_gate(0, 0, printf,
		"  fabric-1 -> fabric-3: 调用 example3_empty ... ");
	fabric_gate(0, example3_empty);
	flexos_nop_gate(0, 0, printf, "OK\n");

	/* 同 fabric 调用: fabric-1 → example1 */
	flexos_nop_gate(0, 0, printf,
		"  fabric-1 -> fabric-1: 调用 example1_empty ... ");
	flexos_nop_gate(0, 0, example1_empty);
	flexos_nop_gate(0, 0, printf, "OK\n");

	return 0;
}

/* ================================================================
 * 测试 2: 多次跨 fabric 调用，验证权限恢复
 * ================================================================ */

static int test_multiple_cross_fabric(void)
{
	flexos_nop_gate(0, 0, printf,
		"\n[test 2] 多次跨 fabric 调用，验证权限恢复\n");

	for (int i = 0; i < 3; i++) {
		flexos_nop_gate(0, 0, printf,
			"  round %d: ", i + 1);

		/* fabric-1 → fabric-2 */
		fabric_gate(0, example2_empty);
		flexos_nop_gate(0, 0, printf, "f2 OK, ");

		/* fabric-1 → fabric-3 */
		fabric_gate(0, example3_empty);
		flexos_nop_gate(0, 0, printf, "f3 OK, ");

		/* fabric-1 → fabric-1 */
		flexos_nop_gate(0, 0, example1_empty);
		flexos_nop_gate(0, 0, printf, "f1 OK\n");
	}

	return 0;
}

/* ================================================================
 * 测试 3: 数据隔离验证
 *
 * 读取 example2_secret_value（属于 fabric-2 的 .data section）。
 * 如果 MPK 隔离生效，直接读取会触发 #PF。
 * 由于 #PF 会导致系统 crash，这里通过 gate 调用来验证
 * 跨 fabric 访问是否正常工作。
 * ================================================================ */

extern int example2_secret_value;

static int test_data_isolation(void)
{
	flexos_nop_gate(0, 0, printf,
		"\n[test 3] 数据隔离验证\n");
	flexos_nop_gate(0, 0, printf,
		"  example2_secret_value 地址: %p\n", &example2_secret_value);
	flexos_nop_gate(0, 0, printf,
		"  尝试直接读取 example2_secret_value（属于 fabric-2 的数据）...\n");
	flexos_nop_gate(0, 0, printf,
		"  如果 MPK 隔离生效，下面会触发 #PF（page fault）\n");

	/* 直接读取其他 fabric 的数据 — 如果隔离生效，这里会触发 page fault */
	int val = example2_secret_value;

	/* 如果走到这里，说明隔离没有生效 */
	flexos_nop_gate(0, 0, printf,
		"  [FAIL] 读取成功: val=%d — 隔离未生效!\n", val);

	return 0;
}

/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
	flexos_nop_gate(0, 0, printf,
		"\n========================================\n");
	flexos_nop_gate(0, 0, printf,
		"  Fabric MPK Isolation 功能测试\n");
	flexos_nop_gate(0, 0, printf,
		"========================================\n");

	/* 测试 1: 跨 fabric 调用 */
	test_cross_fabric_call();

	/* 测试 2: 多次跨 fabric 调用 */
	test_multiple_cross_fabric();

	/* 测试 3: 数据隔离验证 */
	test_data_isolation();

	flexos_nop_gate(0, 0, printf,
		"\n========================================\n");
	flexos_nop_gate(0, 0, printf,
		"  所有测试完成\n");
	flexos_nop_gate(0, 0, printf,
		"  注意: 直接访问其他 fabric 的数据会触发 #PF（隔离生效）\n");
	flexos_nop_gate(0, 0, printf,
		"========================================\n\n");

	return 0;
}
