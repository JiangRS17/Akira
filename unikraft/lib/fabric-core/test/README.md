# Fabric Core 测试

## 概述

本目录包含 Fabric Core 组件的原生（native）测试环境。由于 Fabric Core 依赖 Unikraft 框架的 API（如 `uk_zalloc`、`uk_pr_info` 等），无法直接在用户态编译运行，因此通过 mock stub 头文件模拟 Unikraft API，使整个 demo 可以在标准 Linux 环境下用 `gcc` 编译并运行。

## 目录结构

```
test/
├── Makefile          # 编译和运行脚本
├── README.md         # 本文件
└── stubs/            # Mock 头文件，替代 Unikraft 系统头
    ├── flexos/
    │   ├── isolation.h   # flexos_shared_alloc 定义（引用 uk/alloc.h）
    │   └── literals.h    # FLEXOS_SHARED_LITERAL 宏（直接返回字符串）
    └── uk/
        ├── alloc.h       # uk_zalloc / uk_free 等（映射到 calloc/free）
        ├── mutex.h       # uk_mutex（映射到 pthread_mutex）
        ├── print.h       # uk_pr_info / uk_pr_err（映射到 printf/fprintf）
        └── sched.h       # uk_sched_yield（映射到 sched_yield）
```

## 编译与运行

```bash
cd unikraft/lib/fabric-core/test

# 编译
make

# 编译并运行
make run

# 清理
make clean
```

## 测试拓扑

测试创建三条软总线（Fabric），建立双向桥接：

```
           Fabric 0 (root)
           /            \
    Fabric 1 (left)   Fabric 2 (right)
```

每条边是**双向**的：
- `root` 的 bridge downstream 指向 `left` 和 `right`
- `left` 的 bridge downstream 指回 `root`
- `right` 的 bridge downstream 指回 `root`

## 注册的服务函数

| Fabric | 服务名 | 参数 | 返回值 | 功能 |
|--------|--------|------|--------|------|
| root (0) | `greet` | `const char *name` (1个) | void | 打印问候语 |
| root (0) | `fibonacci` | `int n` (1个) | long | 计算第 n 项斐波那契数 |
| root (0) | `print_status` | `int code, const char *msg` (2个) | void | 打印状态码和消息 |
| left (1) | `add` | `int a, int b` (2个) | long | 两数相加 |
| left (1) | `clamp` | `int val, int lo, int hi` (3个) | long | 将值限制在 [lo, hi] 范围 |
| left (1) | `shout` | `const char *msg, int times` (2个) | void | 将消息重复打印 n 次 |
| right (2) | `multiply` | `int a, int b` (2个) | long | 两数相乘 |
| right (2) | `log_message` | `int level, const char *tag, const char *msg` (3个) | void | 带级别和标签的日志输出 |
| right (2) | `is_even` | `int x` (1个) | long | 判断是否为偶数 |

## 测试步骤与结果

### Step 1: 创建三条软总线
验证 `fabric_create()` 正确创建 3 条 Fabric，ID 依次分配为 0、1、2。
**结果：** 6/6 PASS

### Step 2: 建立双向桥接
验证 `fabric_bridge_bind_downstream()` 双向绑定：
- root → left, left → root
- root → right, right → root
- 检查各 Fabric 的 `downstream_count`
**结果：** 7/7 PASS

### Step 3: 设置组件状态为 RUNNING
将各 Fabric 的 component 状态设为 `FB_COMP_STATE_RUNNING`（否则消息派发会拒绝执行）。
**结果：** 3/3 PASS

### Step 4: 注册服务函数
在 3 条 Fabric 上注册 9 个服务函数，并验证重复注册（覆盖语义）。
**结果：** 10/10 PASS

### Step 5: 同 Fabric 本地调用
每个 Fabric 调用自己注册的服务，验证本地调用路径：
- root: `fibonacci(10)` = 55
- left: `add(10, 20)` = 30, `clamp(100, 0, 50)` = 50
- right: `multiply(6, 7)` = 42, `is_even(42)` = 1
**结果：** 5/5 PASS

### Step 6: 跨 Fabric DFS 路由调用
验证 DFS 路由算法在不同路径上的正确性：

| 调用路径 | 调用 | 预期结果 | 说明 |
|----------|------|----------|------|
| root → left | `add(100, 200)` | 300 | 单跳：root 没有 → left 有 |
| root → right | `multiply(8, 9)` | 72 | 单跳：root 没有 → left 没有 → right 有 |
| left → root | `fibonacci(12)` | 144 | 单跳：left 没有 → root 有 |
| left → root → right | `is_even(7)` | 0 | 双跳：left → root(已访问) → right |
| right → root → left | `clamp(-5, 0, 100)` | 0 | 双跳：right → root → left |
| right → root | `fibonacci(15)` | 610 | 单跳：right → root |

**结果：** 6/6 PASS

### Step 7: 无返回值 (void) 调用
验证 `fabric_gate`（无返回值版本）的跨 Fabric 调用：
- `greet("Alice")`, `log_message(...)`, `shout(...)`, `print_status(...)`
**结果：** 全部正常执行，日志输出正确

### Step 8: 函数注销
验证 `fabric_unregister_function()` 的各种边界情况：
- 注销后再调用：dispatch 失败，返回值保持原样（不被覆盖）
- 重复注销同一函数：返回 `FABRIC_ERROR`
- 注销不存在的函数：返回 `FABRIC_ERROR`
- 本地注销后，自己也调不到
**结果：** 6/6 PASS

### Step 9: 全局映射表验证
验证 `g_fabrics[]` 全局数组正确指向已创建的 Fabric。
**结果：** 3/3 PASS

### Step 10: 重新注册并再次跨 Fabric 调用
注销后重新注册，验证跨 Fabric 调用恢复正常。
**结果：** 4/4 PASS

### Step 11: 销毁所有资源
验证 `fabric_destroy()` 释放所有子组件，并清空全局表。
**结果：** 6/6 PASS

## 最终结果

```
Results: 56 / 56 tests passed.
```

所有功能验证通过，包括：
- ✅ 软总线创建与销毁
- ✅ 双向桥接拓扑
- ✅ 服务函数注册/注销/重注册
- ✅ 本地调用（同 Fabric）
- ✅ 跨 Fabric DFS 路由（单跳、多跳）
- ✅ 有返回值调用（`fabric_gate_r`）
- ✅ 无返回值调用（`fabric_gate`）
- ✅ 参数类型多样化（int、const char *、混合）
- ✅ 参数个数可变（1~3 个，不超过 16 个）
- ✅ dispatch 失败时的错误处理
- ✅ 资源清理与全局状态一致性

## 添加新测试

在 `main.c` 中：
1. 定义服务函数（参数通过 `long` 传递，需在函数内部 cast）
2. 用 `fabric_register_function()` 注册
3. 用 `fabric_gate_r()` / `fabric_gate()` 调用
4. 用 `check()` 验证结果