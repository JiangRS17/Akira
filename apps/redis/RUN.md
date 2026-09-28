# Redis 测试命令

## 配置与测试结果

| ID | 域数 | 隔离布局 | 软件加固 | 说明 |
|----|------|----------|----------|------|
| r01-1d-mpk-none | 1 | monolith | 无 | 基线 MPK |
| r02-2d-all-harden | 2 | redis+newlib \| uksched+lwip | 全部 4 组件 | |
| r03-3d-none | 3 | redis+newlib \| uksched \| lwip | 无 | |
| r04-3d-redis-harden | 3 | redis+newlib \| uksched \| lwip | 仅 Redis | |
| r05-2d-redis-harden | 2 | redis+newlib \| uksched+lwip | 仅 Redis | |
| r06-2d-3harden | 2 | redis+newlib \| uksched+lwip | Redis+uksched+lwip | |
| r07-2d-all-harden | 2 | redis+newlib \| uksched+lwip | 全部 4 组件 | |

结果文件：`benchmark/results.csv`（含 HARDENING 列）

---

```bash
cd /root/.unikraft_akira/apps/redis
```

## 跑全部 7 种配置

```bash
./benchmark/run_matrix.sh
```

## 跑单个配置

```bash
./benchmark/run_matrix.sh r01-1d-mpk-none
```

## 跳过重建

```bash
SKIP_BUILD=1 ./benchmark/run_matrix.sh
```

## 查看结果

```bash
cat benchmark/results.csv
tail -f benchmark/matrix_run.log
```
