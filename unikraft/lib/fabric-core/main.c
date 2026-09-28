/*
 * Fabric Core Demo
 *
 * 拓扑结构（双向边）：
 *
 *          Fabric 0 (root)
 *          /            \
 *   Fabric 1 (left)   Fabric 2 (right)
 *
 * 每条边是双向的：root 的 bridge downstream 指向 left/right，
 * left/right 的 bridge downstream 也指回 root。
 *
 * 功能测试：
 *   1. 创建三条软总线并建立双向桥接
 *   2. 在不同 fabric 上注册服务函数（参数类型、个数各异）
 *   3. 跨 fabric 调用（有返回值 / 无返回值）
 *   4. 函数注册与注销
 *   5. 资源销毁
 */

#include "./fabric/include/fabric.h"
#include "./controller/include/controller.h"
#include "./bridge/include/bridge.h"
#include "./component/include/component.h"
#include "./map/include/map.h"
#include "./include/fabric_error.h"
#include "./include/fabric/isolation.h"
#include "./backends/include/backends.h"

#include <uk/print.h>

/* ======================== Fabric 0 (root) 的服务 ======================== */

/*
 * greet - 向指定名字打招呼（void，1 个参数）
 */
static void greet(const char *name)
{
	uk_pr_info("  [root.greet] Hello, %s! Welcome to Fabric Core.\n",
		   name ? name : "(null)");
}

/*
 * fibonacci - 计算第 n 项斐波那契数（有返回值，1 个参数）
 */
static long fibonacci(int n)
{
	long a = 0, b = 1, tmp;

	uk_pr_info("  [root.fibonacci] Computing fib(%d)...\n", n);

	if (n <= 0)
		return 0;
	if (n == 1)
		return 1;

	for (int i = 2; i <= n; i++) {
		tmp = a + b;
		a = b;
		b = tmp;
	}
	uk_pr_info("  [root.fibonacci] fib(%d) = %ld\n", n, b);
	return b;
}

/*
 * print_status - 打印一条状态码+消息（void，2 个参数）
 */
static void print_status(int code, const char *msg)
{
	uk_pr_info("  [root.print_status] code=%d, msg=\"%s\"\n",
		   code, msg ? msg : "(null)");
}

/* ======================== Fabric 1 (left) 的服务 ======================== */

/*
 * add - 两数相加（有返回值，2 个参数）
 */
static long add(int a, int b)
{
	long result = (long)a + (long)b;
	uk_pr_info("  [left.add] %d + %d = %ld\n", a, b, result);
	return result;
}

/*
 * clamp - 将值限制在 [lo, hi] 范围内（有返回值，3 个参数）
 */
static long clamp(int val, int lo, int hi)
{
	long result;

	if (val < lo)      result = lo;
	else if (val > hi) result = hi;
	else               result = val;

	uk_pr_info("  [left.clamp] clamp(%d, %d, %d) = %ld\n",
		   val, lo, hi, result);
	return result;
}

/*
 * shout - 将消息打印 n 次（void，2 个参数）
 */
static void shout(const char *msg, int times)
{
	uk_pr_info("  [left.shout] Repeating \"%s\" %d times:\n",
		   msg ? msg : "(null)", times);
	for (int i = 0; i < times && i < 10; i++)
		uk_pr_info("    %d: %s\n", i + 1, msg ? msg : "(null)");
}

/* ======================== Fabric 2 (right) 的服务 ======================== */

/*
 * multiply - 两数相乘（有返回值，2 个参数）
 */
static long multiply(int a, int b)
{
	long result = (long)a * (long)b;
	uk_pr_info("  [right.multiply] %d * %d = %ld\n", a, b, result);
	return result;
}

/*
 * log_message - 带级别和标签的日志输出（void，3 个参数）
 */
static void log_message(int level, const char *tag, const char *msg)
{
	const char *level_str;

	switch (level) {
	case 0: level_str = "DEBUG"; break;
	case 1: level_str = "INFO";  break;
	case 2: level_str = "WARN";  break;
	case 3: level_str = "ERROR"; break;
	default: level_str = "???";  break;
	}

	uk_pr_info("  [right.log] [%s][%s] %s\n",
		   level_str,
		   tag ? tag : "default",
		   msg ? msg : "");
}

/*
 * is_even - 判断是否为偶数（有返回值，1 个参数）
 */
static long is_even(int x)
{
	long result = ((x & 1) == 0) ? 1 : 0;
	uk_pr_info("  [right.is_even] %d is %s\n",
		   x, result ? "even" : "odd");
	return result;
}

/* ======================== 辅助函数 ======================== */

static int test_counter = 0;
static int pass_counter = 0;

static void check(const char *name, int condition)
{
	test_counter++;
	if (condition) {
		pass_counter++;
		uk_pr_info("[PASS] %s\n", name);
	} else {
		uk_pr_info("[FAIL] %s\n", name);
	}
}

/* ======================== 主入口 ======================== */

int main(int argc, char *argv[])
{
	Fabric *root, *left, *right;
	long ret = 0;

	(void)argc;
	(void)argv;

	uk_pr_info("========== Fabric Core Demo Start ==========\n\n");

	/* ============================================================
	 * Step 1: 创建三条软总线
	 * ============================================================ */
	uk_pr_info("--- Step 1: Create 3 fabrics ---\n");
	root  = fabric_create();
	left  = fabric_create();
	right = fabric_create();

	check("root  fabric created",  root  != NULL);
	check("left  fabric created",  left  != NULL);
	check("right fabric created",  right != NULL);

	if (!root || !left || !right) {
		uk_pr_info("Cannot continue without all fabrics.\n");
		return -1;
	}

	check("root  id == 0", root->fabric_id  == 0);
	check("left  id == 1", left->fabric_id  == 1);
	check("right id == 2", right->fabric_id == 2);

	/* ============================================================
	 * Step 2: 建立双向桥接
	 * ============================================================ */
	uk_pr_info("\n--- Step 2: Bind bidirectional bridges ---\n");

	/* root <-> left: 双向 */
	check("root->left downstream bind",
	      fabric_bridge_bind_downstream(root->bridge, left) == FABRIC_SUCCESS);
	check("left->root downstream bind",
	      fabric_bridge_bind_downstream(left->bridge, root) == FABRIC_SUCCESS);

	/* root <-> right: 双向 */
	check("root->right downstream bind",
	      fabric_bridge_bind_downstream(root->bridge, right) == FABRIC_SUCCESS);
	check("right->root downstream bind",
	      fabric_bridge_bind_downstream(right->bridge, root) == FABRIC_SUCCESS);

	check("root downstream_count == 2",
	      root->bridge->downstream_count == 2);
	check("left  downstream_count == 1",
	      left->bridge->downstream_count == 1);
	check("right downstream_count == 1",
	      right->bridge->downstream_count == 1);

	/* ============================================================
	 * Step 2.5: 设置隔离后端
	 * ============================================================ */
	uk_pr_info("\n--- Step 2.5: Setup isolation backend ---\n");

	/* [旧设计] 每个 bridge 独立指定 upstream_domain */
	// MPKConfig root_cfg = { .upstream_domain = 0, .isolation_level = 0 };
	// MPKConfig left_cfg = { .upstream_domain = 1, .isolation_level = 0 };
	// MPKConfig right_cfg = { .upstream_domain = 2, .isolation_level = 0 };

	/* [新设计] 每个 fabric 通过 fabric_id 全局分配 MPK key */
	MPKConfig root_cfg  = { .fabric_id = root->fabric_id };
	MPKConfig left_cfg  = { .fabric_id = left->fabric_id };
	MPKConfig right_cfg = { .fabric_id = right->fabric_id };

	check("root bridge set isolation (mpk)",
	      fabric_bridge_set_isolation(root->bridge,
	        FABRIC_ISO_MPK, &root_cfg) == FABRIC_SUCCESS);

	/* 验证后端已挂载 */
	check("root bridge iso_backend set",
	      root->bridge->iso_backend != NULL);
	uk_pr_info("  root bridge backend: %s\n",
		   root->bridge->iso_backend ?
		   root->bridge->iso_backend->name : "(null)");

	check("left bridge set isolation (mpk)",
	      fabric_bridge_set_isolation(left->bridge,
	        FABRIC_ISO_MPK, &left_cfg) == FABRIC_SUCCESS);

	check("right bridge set isolation (mpk)",
	      fabric_bridge_set_isolation(right->bridge,
	        FABRIC_ISO_MPK, &right_cfg) == FABRIC_SUCCESS);

	/* ============================================================
	 * Step 3: 验证组件状态为 RUNNING（fabric_create 自动设置）
	 * ============================================================ */
	uk_pr_info("\n--- Step 3: Verify component status (auto-set to RUNNING) ---\n");

	check("root  component RUNNING",
	      root->component->status == FB_COMP_STATE_RUNNING);
	check("left  component RUNNING",
	      left->component->status == FB_COMP_STATE_RUNNING);
	check("right component RUNNING",
	      right->component->status == FB_COMP_STATE_RUNNING);

	/* ============================================================
	 * Step 4: 注册服务函数（每条 fabric 2~3 个，参数各异）
	 * ============================================================ */
	uk_pr_info("\n--- Step 4: Register service functions ---\n");

	/* Fabric 0 (root): greet(1 arg, void), fibonacci(1 arg, ret), print_status(2 args, void) */
	check("register greet on root",
	      fabric_register_function(root, "greet",
				      (func_t)greet) == FABRIC_SUCCESS);
	check("register fibonacci on root",
	      fabric_register_function(root, "fibonacci",
				      (func_t)fibonacci) == FABRIC_SUCCESS);
	check("register print_status on root",
	      fabric_register_function(root, "print_status",
				      (func_t)print_status) == FABRIC_SUCCESS);

	/* Fabric 1 (left): add(2 args, ret), clamp(3 args, ret), shout(2 args, void) */
	check("register add on left",
	      fabric_register_function(left, "add",
				      (func_t)add) == FABRIC_SUCCESS);
	check("register clamp on left",
	      fabric_register_function(left, "clamp",
				      (func_t)clamp) == FABRIC_SUCCESS);
	check("register shout on left",
	      fabric_register_function(left, "shout",
				      (func_t)shout) == FABRIC_SUCCESS);

	/* Fabric 2 (right): multiply(2 args, ret), log_message(3 args, void), is_even(1 arg, ret) */
	check("register multiply on right",
	      fabric_register_function(right, "multiply",
				      (func_t)multiply) == FABRIC_SUCCESS);
	check("register log_message on right",
	      fabric_register_function(right, "log_message",
				      (func_t)log_message) == FABRIC_SUCCESS);
	check("register is_even on right",
	      fabric_register_function(right, "is_even",
				      (func_t)is_even) == FABRIC_SUCCESS);

	/* 重复注册同一函数名应覆盖（Update 语义） */
	check("re-register add on left (overwrite)",
	      fabric_register_function(left, "add",
				      (func_t)add) == FABRIC_SUCCESS);

	/* ============================================================
	 * Step 5: 同 fabric 调用（本地调用）
	 * ============================================================ */
	uk_pr_info("\n--- Step 5: Same-fabric calls (local) ---\n");

	/* root 调用自己注册的 fibonacci(10) */
	uk_pr_info(">> root calls fibonacci(10):\n");
	ret = 0;
	fabric_gate_r(root->fabric_id, ret, fibonacci, 10);
	check("root calls fibonacci(10) == 55", ret == 55);

	/* left 调用自己注册的 add(10, 20) */
	uk_pr_info(">> left calls add(10, 20):\n");
	ret = 0;
	fabric_gate_r(left->fabric_id, ret, add, 10, 20);
	check("left calls add(10,20) == 30", ret == 30);

	/* left 调用自己注册的 clamp(100, 0, 50) */
	uk_pr_info(">> left calls clamp(100, 0, 50):\n");
	ret = 0;
	fabric_gate_r(left->fabric_id, ret, clamp, 100, 0, 50);
	check("left calls clamp(100,0,50) == 50", ret == 50);

	/* right 调用自己注册的 multiply(6, 7) */
	uk_pr_info(">> right calls multiply(6, 7):\n");
	ret = 0;
	fabric_gate_r(right->fabric_id, ret, multiply, 6, 7);
	check("right calls multiply(6,7) == 42", ret == 42);

	/* right 调用自己注册的 is_even(42) */
	uk_pr_info(">> right calls is_even(42):\n");
	ret = 0;
	fabric_gate_r(right->fabric_id, ret, is_even, 42);
	check("right calls is_even(42) == 1 (even)", ret == 1);

	/* ============================================================
	 * Step 6: 跨 fabric 调用（DFS 路由）
	 * ============================================================ */
	uk_pr_info("\n--- Step 6: Cross-fabric calls (DFS routing) ---\n");

	/*
	 * root -> left: 调用 add(100, 200)
	 * 路由: root 没有 add -> downstream[left] 有 -> 返回 300
	 */
	uk_pr_info(">> root -> left, call add(100, 200):\n");
	ret = 0;
	fabric_gate_r(root->fabric_id, ret, add, 100, 200);
	check("root -> left add(100,200) == 300", ret == 300);

	/*
	 * root -> right: 调用 multiply(8, 9)
	 * 路由: root 没有 -> downstream[left] 没有 -> downstream[right] 有
	 */
	uk_pr_info(">> root -> right, call multiply(8, 9):\n");
	ret = 0;
	fabric_gate_r(root->fabric_id, ret, multiply, 8, 9);
	check("root -> right multiply(8,9) == 72", ret == 72);

	/*
	 * left -> root: 调用 fibonacci(12)
	 * 路由: left 没有 -> downstream[root] 有
	 */
	uk_pr_info(">> left -> root, call fibonacci(12):\n");
	ret = 0;
	fabric_gate_r(left->fabric_id, ret, fibonacci, 12);
	check("left -> root fibonacci(12) == 144", ret == 144);

	/*
	 * left -> root -> right: 调用 is_even(7)
	 * 路由: left 没有 -> root 没有 -> root 的 downstream: left(已访问) -> right 有
	 */
	uk_pr_info(">> left -> root -> right, call is_even(7):\n");
	ret = 0;
	fabric_gate_r(left->fabric_id, ret, is_even, 7);
	check("left -> root -> right is_even(7) == 0 (odd)", ret == 0);

	/*
	 * right -> root -> left: 调用 clamp(-5, 0, 100)
	 * 路由: right 没有 -> root 没有 -> root 的 downstream: left 有
	 */
	uk_pr_info(">> right -> root -> left, call clamp(-5, 0, 100):\n");
	ret = 0;
	fabric_gate_r(right->fabric_id, ret, clamp, -5, 0, 100);
	check("right -> root -> left clamp(-5,0,100) == 0", ret == 0);

	/*
	 * right -> root: 调用 fibonacci(15)
	 */
	uk_pr_info(">> right -> root, call fibonacci(15):\n");
	ret = 0;
	fabric_gate_r(right->fabric_id, ret, fibonacci, 15);
	check("right -> root fibonacci(15) == 610", ret == 610);

	/* ============================================================
	 * Step 7: 无返回值调用（void）
	 * ============================================================ */
	uk_pr_info("\n--- Step 7: Void calls (no return value) ---\n");

	/* left -> root: greet("Alice") */
	uk_pr_info(">> left -> root, call greet(\"Alice\"):\n");
	fabric_gate(left->fabric_id, greet, "Alice");

	/* root -> right: log_message(1, "net", "packet received") */
	uk_pr_info(">> root -> right, call log_message(1, \"net\", \"packet received\"):\n");
	fabric_gate(root->fabric_id, log_message, 1, "net", "packet received");

	/* right -> root -> left: shout("Fabric is awesome!", 3) */
	uk_pr_info(">> right -> root -> left, call shout(\"Fabric is awesome!\", 3):\n");
	fabric_gate(right->fabric_id, shout, "Fabric is awesome!", 3);

	/* root -> left: shout("hello", 2) */
	uk_pr_info(">> root -> left, call shout(\"hello\", 2):\n");
	fabric_gate(root->fabric_id, shout, "hello", 2);

	/* left -> root: print_status(200, "OK") */
	uk_pr_info(">> left -> root, call print_status(200, \"OK\"):\n");
	fabric_gate(left->fabric_id, print_status, 200, "OK");

	/* right -> root: greet("Bob") */
	uk_pr_info(">> right -> root, call greet(\"Bob\"):\n");
	fabric_gate(right->fabric_id, greet, "Bob");

	/* ============================================================
	 * Step 8: 函数注销
	 * ============================================================ */
	uk_pr_info("\n--- Step 8: Unregister function ---\n");

	check("unregister multiply from right",
	      fabric_unregister_function(right, "multiply") == FABRIC_SUCCESS);

	/* 注销后再跨 fabric 调用应失败：dispatch 失败，ret 保持原值（未被覆盖） */
	ret = -1;
	fabric_gate_r(root->fabric_id, ret, multiply, 8, 9);
	check("after unregister, multiply dispatch fails (ret unchanged)", ret == -1);

	/* 重复注销应失败 */
	check("double unregister multiply fails",
	      fabric_unregister_function(right, "multiply") == FABRIC_ERROR);

	/* 注销不存在的函数应失败 */
	check("unregister non-existent func fails",
	      fabric_unregister_function(right, "no_such_func") == FABRIC_ERROR);

	/* 本地注销后，自己也调不到了：dispatch 失败，ret 保持原值 */
	check("unregister add from left",
	      fabric_unregister_function(left, "add") == FABRIC_SUCCESS);
	ret = -1;
	fabric_gate_r(left->fabric_id, ret, add, 1, 2);
	check("after local unregister, add dispatch fails (ret unchanged)", ret == -1);

	/* ============================================================
	 * Step 9: 全局映射表验证
	 * ============================================================ */
	uk_pr_info("\n--- Step 9: Global fabric table verification ---\n");

	check("g_fabrics[0] == root",  g_fabrics[0] == root);
	check("g_fabrics[1] == left",  g_fabrics[1] == left);
	check("g_fabrics[2] == right", g_fabrics[2] == right);

	/* ============================================================
	 * Step 10: 重新注册并再次跨 fabric 调用
	 * ============================================================ */
	uk_pr_info("\n--- Step 10: Re-register and call again ---\n");

	check("re-register multiply on right",
	      fabric_register_function(right, "multiply",
				      (func_t)multiply) == FABRIC_SUCCESS);
	check("re-register add on left",
	      fabric_register_function(left, "add",
				      (func_t)add) == FABRIC_SUCCESS);

	uk_pr_info(">> left -> right, call multiply(12, 12):\n");
	ret = 0;
	fabric_gate_r(left->fabric_id, ret, multiply, 12, 12);
	check("left -> root -> right multiply(12,12) == 144", ret == 144);

	uk_pr_info(">> right -> left, call add(500, 501):\n");
	ret = 0;
	fabric_gate_r(right->fabric_id, ret, add, 500, 501);
	check("right -> root -> left add(500,501) == 1001", ret == 1001);

	/* ============================================================
	 * Step 11: 清理 — 销毁所有资源
	 * ============================================================ */
	uk_pr_info("\n--- Step 11: Destroy all fabrics ---\n");

	check("destroy left",  fabric_destroy(left)  == FABRIC_SUCCESS);
	check("destroy right", fabric_destroy(right) == FABRIC_SUCCESS);
	check("destroy root",  fabric_destroy(root)  == FABRIC_SUCCESS);

	check("g_fabrics[0] == NULL after destroy", g_fabrics[0] == NULL);
	check("g_fabrics[1] == NULL after destroy", g_fabrics[1] == NULL);
	check("g_fabrics[2] == NULL after destroy", g_fabrics[2] == NULL);

	/* ============================================================
	 * 汇总
	 * ============================================================ */
	uk_pr_info("\n========== Fabric Core Demo End ==========\n");
	uk_pr_info("Results: %d / %d tests passed.\n",
		   pass_counter, test_counter);

	return (pass_counter == test_counter) ? 0 : 1;
}