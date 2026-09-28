#!/bin/sh
# Build + run bench_domains; write results_domains.csv for plotting.
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-"$APP_DIR/results_domains.csv"}
RAW=${RAW:-"$APP_DIR/results_domains.raw.log"}
MK_UK="$APP_DIR/Makefile.uk"

cd "$APP_DIR"
chmod +x run.sh

# Point app sources at domains bench (restore map bench afterward).
cp -a "$MK_UK" "$MK_UK.bak.domains"
trap 'mv -f "$MK_UK.bak.domains" "$MK_UK"' EXIT
sed -i 's|bench_map\.c|bench_domains.c|;s|bench_grid\.c|bench_domains.c|;s|bench_libs\.c|bench_domains.c|;s|bench_layers\.c|bench_domains.c|' "$MK_UK"
if ! grep -q 'bench_domains\.c' "$MK_UK"; then
	printf '%s\n' \
		'$(eval $(call addlib,appscalability))' \
		'APPSCALABILITY_SRCS-y += $(APPSCALABILITY_BASE)/bench_domains.c' \
		'APPSCALABILITY_SRCS-y += $(APPSCALABILITY_BASE)/helper/scale_topo.c' \
		'APPSCALABILITY_CINCLUDES-y += -I$(APPSCALABILITY_BASE)/helper' \
		'APPSCALABILITY_CINCLUDES-y += -I$(UK_BASE)/lib/fabric-core/overhead/include' \
		> "$MK_UK"
fi
echo "[domains] Makefile.uk:"
cat "$MK_UK"

echo "[domains] kraft configure (intel-pku)..."
yes | kraft configure 2>&1 | tail -20

echo "[domains] building..."
SRC_BO=/root/.unikraft_akira/apps/base-overhead/build
mkdir -p build/libnewlibc build/libtlsf
[ -d build/libnewlibc/origin ] || cp -a "$SRC_BO/libnewlibc/origin" build/libnewlibc/ 2>/dev/null || true
[ -d build/libtlsf/origin ] || cp -a "$SRC_BO/libtlsf/origin" build/libtlsf/ 2>/dev/null || true
cp -n "$SRC_BO/libnewlibc/newlib-2.5.0.20170922.tar.gz" build/libnewlibc/ 2>/dev/null || true
cp -n "$SRC_BO/libtlsf/TLSF-2.4.6.tbz2" build/libtlsf/ 2>/dev/null || true

# Force recompile of switched main + topo helper
rm -f build/appscalability/bench_*.o build/appscalability/helper/*.o 2>/dev/null || true

kraft -v build --no-progress --fast --compartmentalize --no-fetch 2>&1 | tee build_domains.log | tail -40

echo "[domains] running..."
set +e
./run.sh >"$RAW" 2>&1
rc=$?
set -e
if [ "$rc" -ne 0 ] && [ "$rc" -ne 83 ]; then
	echo "[domains] qemu rc=$rc — check $RAW" >&2
fi

printf 'series,N,min_cycles,avg_cycles,dfs_visits,keys,status\n' > "$OUT"
grep -E '^CSV,' "$RAW" | sed 's/^CSV,//' >> "$OUT"

echo "[domains] data: $OUT"
echo "[domains] raw:  $RAW"
wc -l "$OUT"
cat "$OUT"
