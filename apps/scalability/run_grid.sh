#!/bin/sh
# Build + run bench_grid; write results_grid.csv for plotting.
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-"$APP_DIR/results_grid.csv"}
RAW=${RAW:-"$APP_DIR/results_grid.raw.log"}

cd "$APP_DIR"
chmod +x run.sh

echo "[grid] kraft configure (intel-pku)..."
yes | kraft configure 2>&1 | tail -20

echo "[grid] building..."
# Reuse newlib/tlsf origins if wget blocked
SRC_BO=/root/.unikraft_akira/apps/base-overhead/build
mkdir -p build/libnewlibc build/libtlsf
[ -d build/libnewlibc/origin ] || cp -a "$SRC_BO/libnewlibc/origin" build/libnewlibc/ 2>/dev/null || true
[ -d build/libtlsf/origin ] || cp -a "$SRC_BO/libtlsf/origin" build/libtlsf/ 2>/dev/null || true
cp -n "$SRC_BO/libnewlibc/newlib-2.5.0.20170922.tar.gz" build/libnewlibc/ 2>/dev/null || true
cp -n "$SRC_BO/libtlsf/TLSF-2.4.6.tbz2" build/libtlsf/ 2>/dev/null || true

kraft -v build --no-progress --fast --compartmentalize --no-fetch 2>&1 | tee build_grid.log | tail -30

echo "[grid] running (this can take several minutes)..."
set +e
./run.sh >"$RAW" 2>&1
rc=$?
set -e
if [ "$rc" -ne 0 ] && [ "$rc" -ne 83 ]; then
	echo "[grid] qemu rc=$rc — check $RAW" >&2
fi

printf 'series,L,K,min_cycles,avg_cycles,dfs_visits\n' > "$OUT"
grep -E '^CSV,' "$RAW" | sed 's/^CSV,//' >> "$OUT"

echo "[grid] data: $OUT"
echo "[grid] raw:  $RAW"
wc -l "$OUT"
cat "$OUT"
