#!/bin/bash
# 用法: ./benchmark/run_one.sh n01-1d-mpk-none
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$APP_DIR"

ID="${1:?用法: $0 n01-1d-mpk-none}"
OUT="benchmark/results_${ID}.csv"

eval "$(python3 - "$ID" <<'PY'
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

# Keep MPK CSV intact; write EPT to a separate file.
if [[ "$BACKEND" == "ept" ]]; then
	OUT="benchmark/results_${ID}-ept.csv"
	rm -f "$OUT"
fi

./configure.sh "$ID" --build
mkdir -p "benchmark/builds/$ID"
# EPT: copy all .compN images for this kraft compartment count.
if [[ -f build/nginx_kvm-x86_64.comp0 ]]; then
	NCOMP=$(python3 - <<'PY'
import yaml
from pathlib import Path
k = yaml.safe_load(Path("kraft.yaml").read_text()) or {}
print(len(k.get("compartments") or []))
PY
)
	rm -f "benchmark/builds/$ID"/nginx_kvm-x86_64.comp[0-9]* \
		"benchmark/builds/$ID"/nginx_kvm-x86_64.dbg.comp[0-9]*
	local_i=0
	while [[ "$local_i" -lt "$NCOMP" ]]; do
		if [[ -f "build/nginx_kvm-x86_64.comp${local_i}" ]]; then
			cp -f "build/nginx_kvm-x86_64.comp${local_i}" "benchmark/builds/$ID/"
		fi
		if [[ -f "build/nginx_kvm-x86_64.dbg.comp${local_i}" ]]; then
			cp -f "build/nginx_kvm-x86_64.dbg.comp${local_i}" "benchmark/builds/$ID/"
		fi
		local_i=$((local_i + 1))
	done
else
	cp -f build/nginx_kvm-x86_64 "benchmark/builds/$ID/"
fi

./benchmark.sh stop 2>/dev/null || true

RESULTS="$OUT" RESULTS_EXTENDED=1 \
TASKID="$TASKID" CONFIG_ID="$ID" DOMAINS="$DOMAINS" BACKEND="$BACKEND" \
SPLIT="$SPLIT" HARDENING="$HARDENING" \
ITERATIONS="${ITERATIONS:-5}" \
NUM_PARALLEL_CONNS="${NUM_PARALLEL_CONNS:-$([ "$BACKEND" = ept ] && echo 1 || echo 30)}" \
NUM_THREADS="${NUM_THREADS:-$([ "$BACKEND" = ept ] && echo 1 || echo 14)}" \
UNIKERNEL="benchmark/builds/$ID/nginx_kvm-x86_64" \
./benchmark.sh all

echo "写入: $OUT"
