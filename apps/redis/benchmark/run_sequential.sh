#!/bin/bash
# 逐个运行 r01-r07：每个配置独立构建 → benchmark → 追加写入 results.csv
# 模拟手动一个一个跑，配置之间彻底清理 QEMU 状态
set -uo pipefail

APP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BENCH_DIR="$(cd "$(dirname "$0")" && pwd)"
MATRIX="${MATRIX:-$BENCH_DIR/matrix.yaml}"
RESULTS="${RESULTS:-$BENCH_DIR/results.csv}"
BUILDS_DIR="${BUILDS_DIR:-$BENCH_DIR/builds}"
SKIP_BUILD="${SKIP_BUILD:-0}"
ITERATIONS="${ITERATIONS:-5}"
SLEEP_BETWEEN="${SLEEP_BETWEEN:-3}"

# 默认跑 r01-r07
DEFAULT_IDS=(
	r01-1d-mpk-none
	r02-2d-all-harden
	r03-3d-none
	r04-3d-redis-harden
	r05-2d-redis-harden
	r06-2d-3harden
	r07-2d-all-harden
)

usage() {
	cat <<EOF
Usage: $0 [config-id ...]

逐个运行配置，结果追加写入 results.csv（类似手动一个一个跑）。

Environment:
  SKIP_BUILD=1     不重建，使用 benchmark/builds/<id>/redis_kvm-x86_64
  ITERATIONS=5     每个 chunk 重复次数
  RESULTS          CSV 路径（默认 benchmark/results.csv）
  SLEEP_BETWEEN=3  每个配置之间等待秒数（清理端口）

Examples:
  $0
  $0 r01-1d-mpk-none r03-3d-none
  SKIP_BUILD=1 $0
EOF
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

cleanup_qemu() {
	"$APP_DIR/benchmark.sh" stop 2>/dev/null || true
	pkill -f "qemu-system-x86_64.*redis_kvm-x86_64" 2>/dev/null || true
	sleep "$SLEEP_BETWEEN"
}

summarize() {
	[[ -f "$RESULTS" ]] || return 0
	echo ""
	echo "=== 汇总（avg req/s）==="
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
	local out_bin out_dir rc

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

	cleanup_qemu

	if [[ "$SKIP_BUILD" != "1" ]]; then
		echo ">>> 构建 $cfg_id ..."
		if ! "$APP_DIR/configure.sh" "$cfg_id" --build; then
			echo "FAILED (build): $cfg_id" >&2
			cleanup_qemu
			return 1
		fi
		mkdir -p "$out_dir"
		cp -f "$APP_DIR/build/redis_kvm-x86_64" "$out_bin"
		echo "Saved: $out_bin"
	elif [[ ! -f "$out_bin" ]]; then
		echo "SKIP: missing binary $out_bin (set SKIP_BUILD=0 to build)" >&2
		return 1
	fi

	cleanup_qemu

	echo ">>> benchmark $cfg_id ..."
	rc=0
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
		rc=1
		echo "FAILED (benchmark): $cfg_id (see benchmark/qemu.log)" >&2
	fi

	cleanup_qemu
	return "$rc"
}

main() {
	if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
		usage
		exit 0
	fi

	chmod +x "$APP_DIR/configure.sh" "$APP_DIR/benchmark.sh"
	init_results
	mkdir -p "$BUILDS_DIR"

	local ids=()
	if [[ $# -gt 0 ]]; then
		ids=("$@")
	else
		ids=("${DEFAULT_IDS[@]}")
	fi

	local ok=0 fail=0
	for cfg_id in "${ids[@]}"; do
		if [[ ! -f "$APP_DIR/configs/${cfg_id}.yaml" ]]; then
			echo "Unknown config: $cfg_id" >&2
			((fail++)) || true
			continue
		fi
		if run_one "$cfg_id"; then
			((ok++)) || true
			echo "OK: $cfg_id"
		else
			((fail++)) || true
		fi
	done

	echo ""
	echo "Done: $ok succeeded, $fail failed"
	summarize
}

main "$@"
