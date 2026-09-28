#!/bin/bash
# 逐个运行 n01-n07：每个配置独立构建 → benchmark → 写入 results_<id>.csv
set -uo pipefail

APP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BENCH_DIR="$(cd "$(dirname "$0")" && pwd)"
SKIP_BUILD="${SKIP_BUILD:-0}"
ITERATIONS="${ITERATIONS:-5}"
SLEEP_BETWEEN="${SLEEP_BETWEEN:-3}"
LOG="${LOG:-$BENCH_DIR/matrix_run.log}"

DEFAULT_IDS=(
	n01-1d-mpk-none
	n02-2d-all-harden
	n03-3d-none
	n04-3d-nginx-harden
	n05-2d-nginx-harden
	n06-2d-3harden
	n07-2d-all-harden
)

usage() {
	cat <<EOF
Usage: $0 [config-id ...]

逐个运行配置，每个配置写入 results_<id>.csv。

Environment:
  SKIP_BUILD=1     不重建，使用 benchmark/builds/<id>/nginx_kvm-x86_64
  ITERATIONS=5     wrk 重复次数
  SLEEP_BETWEEN=3  每个配置之间等待秒数

Examples:
  $0
  $0 n01-1d-mpk-none n03-3d-none
EOF
}

cleanup_qemu() {
	"$APP_DIR/benchmark.sh" stop 2>/dev/null || true
	bash "$APP_DIR/run_ept.sh" stop 2>/dev/null || true
	pkill -f "qemu-system-x86_64.*nginx_kvm-x86_64" 2>/dev/null || true
	pkill -f "qemu-system-ept.*nginx_kvm-x86_64" 2>/dev/null || true
	sleep "$SLEEP_BETWEEN"
}

run_one_skip_build() {
	local cfg_id="$1"
	local out_bin="$BENCH_DIR/builds/$cfg_id/nginx_kvm-x86_64"
	local out_csv="$BENCH_DIR/results_${cfg_id}.csv"

	if [[ ! -f "$out_bin" && ! -f "${out_bin}.comp0" ]]; then
		echo "missing $out_bin (or .comp0)" >&2
		return 1
	fi

	eval "$(python3 - "$cfg_id" <<'PY'
import sys, yaml
from pathlib import Path
for c in yaml.safe_load(Path("benchmark/matrix.yaml").read_text())["configs"]:
    if c["id"] == sys.argv[1]:
        print(f'TASKID="{c["taskid"]}"')
        print(f'DOMAINS="{c["domains"]}"')
        print(f'BACKEND="{c["backend"]}"')
        print(f'SPLIT="{c["split"]}"')
        print(f'HARDENING="{c.get("hardening", "")}"')
        break
PY
)"

	rm -f "$out_csv"
	RESULTS="$out_csv" RESULTS_EXTENDED=1 \
	TASKID="$TASKID" CONFIG_ID="$cfg_id" DOMAINS="$DOMAINS" BACKEND="$BACKEND" \
	SPLIT="$SPLIT" HARDENING="$HARDENING" \
	ITERATIONS="$ITERATIONS" \
	UNIKERNEL="$out_bin" \
	"$APP_DIR/benchmark.sh" all
}

main() {
	if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
		usage
		exit 0
	fi

	cd "$APP_DIR"
	chmod +x "$APP_DIR/configure.sh" "$APP_DIR/benchmark.sh" \
		"$BENCH_DIR/run_one.sh" "$BENCH_DIR/run_sequential.sh"

	local ids=()
	if [[ $# -gt 0 ]]; then
		ids=("$@")
	else
		ids=("${DEFAULT_IDS[@]}")
	fi

	{
		echo "=== nginx matrix run $(date -Is) ==="
		local ok=0 fail=0
		for cfg_id in "${ids[@]}"; do
			if [[ ! -f "$APP_DIR/configs/${cfg_id}.yaml" ]]; then
				echo "Unknown config: $cfg_id" >&2
				((fail++)) || true
				continue
			fi
			echo ""
			echo "############################################"
			echo "# $cfg_id"
			echo "############################################"
			cleanup_qemu
			local rc=0
			if [[ "$SKIP_BUILD" == "1" ]]; then
				run_one_skip_build "$cfg_id" || rc=1
			else
				ITERATIONS="$ITERATIONS" "$BENCH_DIR/run_one.sh" "$cfg_id" || rc=1
			fi
			cleanup_qemu
			if [[ "$rc" -eq 0 ]]; then
				((ok++)) || true
				echo "OK: $cfg_id"
			else
				((fail++)) || true
				echo "FAILED: $cfg_id"
			fi
		done
		echo ""
		echo "Done: $ok succeeded, $fail failed"
		echo "CSVs:"
		ls -la "$BENCH_DIR"/results_n*.csv 2>/dev/null || true
	} 2>&1 | tee -a "$LOG"
}

main "$@"
