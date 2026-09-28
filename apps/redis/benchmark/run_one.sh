#!/bin/bash
# 用法: ./benchmark/run_one.sh r01-1d-mpk-none
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$APP_DIR"

ID="${1:?用法: $0 r01-1d-mpk-none}"
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

./configure.sh "$ID" --build
mkdir -p "benchmark/builds/$ID"
cp -f build/redis_kvm-x86_64 "benchmark/builds/$ID/"

./benchmark.sh stop 2>/dev/null || true

RESULTS="$OUT" RESULTS_EXTENDED=1 \
TASKID="$TASKID" CONFIG_ID="$ID" DOMAINS="$DOMAINS" BACKEND="$BACKEND" \
SPLIT="$SPLIT" HARDENING="$HARDENING" \
UNIKERNEL="benchmark/builds/$ID/redis_kvm-x86_64" \
./benchmark.sh all

echo "写入: $OUT"
