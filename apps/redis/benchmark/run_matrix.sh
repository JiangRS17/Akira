#!/bin/bash
# FlexOS 风格 Redis 吞吐量矩阵测试
# 遍历 matrix.yaml 中的 7 种配置：构建 → benchmark → 写入 CSV
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BENCH_DIR="$(cd "$(dirname "$0")" && pwd)"
MATRIX="${MATRIX:-$BENCH_DIR/matrix.yaml}"
RESULTS="${RESULTS:-$BENCH_DIR/results.csv}"
BUILDS_DIR="${BUILDS_DIR:-$BENCH_DIR/builds}"
SKIP_BUILD="${SKIP_BUILD:-0}"
BENCHMARK_ONLY="${BENCHMARK_ONLY:-0}"
ITERATIONS="${ITERATIONS:-5}"
CONFIG_FILTER="${CONFIG_FILTER:-}"

usage() {
	cat <<EOF
Usage: $0 [config-id ...]

按 matrix.yaml 跑 Redis 吞吐量测试（FlexOS fig-06 风格）。

每个配置：apply fabric.yaml → build → redis-benchmark → 写 CSV

Environment:
  SKIP_BUILD=1       不重建，使用 builds/<id>/redis_kvm-x86_64
  BENCHMARK_ONLY=1   同 SKIP_BUILD，且跳过缺失二进制的配置
  ITERATIONS=5       每个 chunk 重复次数（默认 5，对齐 FlexOS AE）
  RESULTS            CSV 输出路径
  CONFIG_FILTER      只跑 ID 包含该子串的配置

Examples:
  $0                              # 跑全部 7 种
  $0 c01-monolith-mpk c05-triple-mpk
  SKIP_BUILD=1 $0 c01-monolith-mpk
  CONFIG_FILTER=lwip $0           # 只跑含 lwip 的配置
EOF
}

list_matrix_ids() {
	python3 - "$MATRIX" <<'PY'
import sys
from pathlib import Path
try:
    import yaml
except ImportError:
    yaml = None

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
if yaml is not None:
    data = yaml.safe_load(text) or {}
    for item in data.get("configs", []):
        print(item["id"])
else:
    in_configs = False
    for line in text.splitlines():
        if line.strip() == "configs:":
            in_configs = True
            continue
        if in_configs and line.startswith("  - id:"):
            print(line.split(":", 1)[1].strip())
PY
}

load_config_meta() {
	local cfg_id="$1"
	python3 - "$MATRIX" "$cfg_id" <<'PY'
import sys
from pathlib import Path
try:
    import yaml
except ImportError:
    yaml = None

path = Path(sys.argv[1])
cfg_id = sys.argv[2]
data = yaml.safe_load(path.read_text(encoding="utf-8")) if yaml else {}
for item in data.get("configs", []):
    if item.get("id") == cfg_id:
        print(item.get("taskid", f"redis-{cfg_id}"))
        print(item.get("domains", ""))
        print(item.get("backend", ""))
        print(item.get("split", ""))
        print(item.get("hardening", ""))
        print(item.get("description", ""))
        break
PY
}

init_results() {
	mkdir -p "$(dirname "$RESULTS")"
	if [[ ! -f "$RESULTS" ]]; then
		echo "TASKID,CONFIG_ID,DOMAINS,BACKEND,SPLIT,HARDENING,CHUNK,ITERATION,METHOD,VALUE" > "$RESULTS"
	fi
}

summarize() {
	[[ -f "$RESULTS" ]] || return 0
	echo ""
	echo "=== 矩阵汇总（avg req/s）==="
	awk -F, 'NR > 1 {
		key = $1 "," $7 "," $9
		sum[key] += $10
		n[key]++
	} END {
		for (k in sum)
			printf "%s avg=%.2f req/s (n=%d)\n", k, sum[k]/n[k], n[k]
	}' "$RESULTS" | sort
	echo ""
	echo "Results: $RESULTS"
}

run_one() {
	local cfg_id="$1"
	local meta taskid domains backend split hardening desc
	local out_bin out_dir

	mapfile -t meta < <(load_config_meta "$cfg_id")
	taskid="${meta[0]:-redis-${cfg_id}}"
	domains="${meta[1]:-}"
	backend="${meta[2]:-}"
	split="${meta[3]:-}"
	hardening="${meta[4]:-}"
	desc="${meta[5]:-}"

	out_dir="$BUILDS_DIR/$cfg_id"
	out_bin="$out_dir/redis_kvm-x86_64"

	echo ""
	echo "############################################"
	echo "# $cfg_id"
	echo "# $desc"
	echo "# domains=$domains backend=$backend split=$split hardening=$hardening"
	echo "############################################"

	if [[ "$SKIP_BUILD" != "1" && "$BENCHMARK_ONLY" != "1" ]]; then
		"$APP_DIR/configure.sh" "$cfg_id" --build
		mkdir -p "$out_dir"
		cp -f "$APP_DIR/build/redis_kvm-x86_64" "$out_bin"
		echo "Saved: $out_bin"
	elif [[ ! -f "$out_bin" ]]; then
		if [[ -f "$APP_DIR/build/redis_kvm-x86_64" ]]; then
			mkdir -p "$out_dir"
			cp -f "$APP_DIR/build/redis_kvm-x86_64" "$out_bin"
		else
			echo "SKIP: missing binary for $cfg_id ($out_bin)" >&2
			return 0
		fi
	fi

	if ! RESULTS_EXTENDED=1 \
		TASKID="$taskid" \
		CONFIG_ID="$cfg_id" \
		DOMAINS="$domains" \
		BACKEND="$backend" \
		SPLIT="$split" \
		HARDENING="$hardening" \
		ITERATIONS="$ITERATIONS" \
		RESULTS="$RESULTS" \
		UNIKERNEL="$out_bin" \
		"$APP_DIR/benchmark.sh" all; then
		echo "FAILED: $cfg_id (see benchmark/qemu.log)" >&2
	fi
}

main() {
	if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
		usage
		exit 0
	fi

	chmod +x "$APP_DIR/configure.sh" "$APP_DIR/benchmark.sh"

	local ids=()
	if [[ $# -gt 0 ]]; then
		ids=("$@")
	else
		mapfile -t ids < <(list_matrix_ids)
	fi

	init_results
	mkdir -p "$BUILDS_DIR"

	for cfg_id in "${ids[@]}"; do
		if [[ -n "$CONFIG_FILTER" && "$cfg_id" != *"$CONFIG_FILTER"* ]]; then
			continue
		fi
		if [[ ! -f "$APP_DIR/configs/${cfg_id}.yaml" ]]; then
			echo "Unknown config: $cfg_id" >&2
			exit 1
		fi
		run_one "$cfg_id"
	done

	summarize
}

main "$@"
