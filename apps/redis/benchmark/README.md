# Redis 吞吐量测试指南

参考 **FlexOS ASPLOS'22 fig-06**：从 80 种 compartment permutation 中选取 **7 种有代表性的配置**，自动构建并跑 `redis-benchmark`，结果写入 CSV。

---

## 1. 七种测试配置

定义见 `benchmark/matrix.yaml`：

| ID | 域数 | 后端 | 隔离对象 | 说明 |
|----|------|------|----------|------|
| `c01-monolith-mpk` | 1 | MPK | — | 基线：所有组件同域 |
| `c02-isolate-lwip-mpk` | 2 | MPK | lwip | 隔离 TCP/IP 栈 |
| `c03-isolate-newlib-mpk` | 2 | MPK | newlib | 隔离 libc |
| `c04-isolate-redis-mpk` | 2 | MPK | redis | 隔离应用 |
| `c05-triple-mpk` | 3 | MPK | app\|lwip\|newlib | 三域分离 |
| `c06-monolith-software` | 1 | software | — | 单域 + 软件隔离 |
| `c07-isolate-lwip-software` | 2 | software | lwip | 隔离 lwip + 软件隔离 |

对应 fabric 预设：`configs/c01-monolith-mpk.yaml` … `configs/c07-isolate-lwip-software.yaml`

---

## 2. 前置条件

```bash
cd /root/.unikraft_akira/apps/redis
kraft configure    # 首次需要

apt install -y redis-tools qemu-kvm
```

---

## 3. 一键跑完整矩阵（推荐）

```bash
cd /root/.unikraft_akira/apps/redis
chmod +x benchmark/run_matrix.sh configure.sh benchmark.sh
./benchmark/run_matrix.sh
```

流程（每个配置）：
1. 应用 `configs/<id>.yaml` → `fabric.yaml`
2. `build_fabric.sh` 构建
3. 二进制保存到 `benchmark/builds/<id>/redis_kvm-x86_64`
4. 启动 QEMU → `redis-benchmark` → 写 CSV → 停止 QEMU

只跑部分配置：

```bash
./benchmark/run_matrix.sh c01-monolith-mpk c02-isolate-lwip-mpk
./benchmark/run_matrix.sh c01-monolith-mpk c05-triple-mpk c06-monolith-software
```

已有构建产物、跳过重建：

```bash
SKIP_BUILD=1 ./benchmark/run_matrix.sh
```

---

## 4. 单配置手动测试

```bash
cd /root/.unikraft_akira/apps/redis

# 应用 + 构建
./configure.sh c01-monolith-mpk --build

# 单次 benchmark
TASKID=redis-c01-monolith-mpk \
CONFIG_ID=c01-monolith-mpk DOMAINS=1 BACKEND=mpk SPLIT=monolith \
RESULTS_EXTENDED=1 ./benchmark.sh all
```

---

## 5. 结果 CSV

路径：`benchmark/results.csv`

扩展格式（`run_matrix.sh` 默认）：

```csv
TASKID,CONFIG_ID,DOMAINS,BACKEND,SPLIT,CHUNK,ITERATION,METHOD,VALUE
redis-c01-monolith-mpk,c01-monolith-mpk,1,mpk,monolith,5,1,SET,980392.19
redis-c01-monolith-mpk,c01-monolith-mpk,1,mpk,monolith,5,1,GET,917431.19
```

| 列 | 含义 |
|----|------|
| `TASKID` | 配置标签 |
| `CONFIG_ID` | 预设 ID |
| `DOMAINS` | fabric 数量 |
| `BACKEND` | `mpk` / `software` |
| `SPLIT` | 隔离对象 |
| `CHUNK` | value 大小（5 / 50 / 500） |
| `ITERATION` | 重复轮次 |
| `METHOD` | `SET` / `GET` |
| `VALUE` | requests/sec |

汇总查看：

```bash
awk -F, 'NR>1 {
  key=$1","$6","$8; sum[key]+=$9; n[key]++
} END {
  for (k in sum) printf "%s avg=%.2f req/s (n=%d)\n", k, sum[k]/n[k], n[k]
}' benchmark/results.csv | sort
```

---

## 6. Benchmark 参数（对齐 FlexOS AE）

| 变量 | 默认 | 说明 |
|------|------|------|
| `NUM_REQUESTS` | 100000 | 总请求数 |
| `NUM_CLIENTS` | 30 | 并发客户端 |
| `PIPELINE` | 16 | pipeline 深度 |
| `CHUNKS` | 5 50 500 | value 大小 |
| `ITERATIONS` | 5（矩阵）/ 3（单次） | 重复次数 |

快速试跑：

```bash
ITERATIONS=1 CHUNKS=5 ./benchmark/run_matrix.sh c01-monolith-mpk
```

---

## 7. 与 FlexOS AE 脚本的对应关系

| FlexOS AE (`test.sh`) | 本项目 |
|-----------------------|--------|
| 遍历 `DIR/*` 子目录 | 遍历 `matrix.yaml` 7 种配置 |
| 每个子目录一个 unikernel 镜像 | `benchmark/builds/<id>/` |
| `TASKID=basename(DIR)` | `TASKID=redis-c01-...` |
| `redis-benchmark -t get,set` | 相同 |
| `results.txt` CSV | `benchmark/results.csv` |

---

## 8. 文件结构

```
apps/redis/
├── configure.sh              # 应用单个预设
├── configs/
│   ├── c01-monolith-mpk.yaml
│   └── ... (共 7 个)
├── benchmark.sh              # 单次 benchmark
└── benchmark/
    ├── README.md             # 本文档
    ├── matrix.yaml           # 7 种配置清单
    ├── run_matrix.sh         # 主测试入口
    ├── builds/<id>/          # 各配置二进制缓存
    ├── results.csv           # 测试结果
    └── qemu.log
```

---

## 9. 快速命令

```bash
./benchmark/run_matrix.sh                    # 跑全部 7 种
./benchmark/run_matrix.sh c01-monolith-mpk     # 只跑基线
SKIP_BUILD=1 ./benchmark/run_matrix.sh         # 不重建
CONFIG_FILTER=lwip ./benchmark/run_matrix.sh   # 只跑含 lwip 的
./configure.sh c01-monolith-mpk --build        # 手动构建单个
```
