#!/bin/sh
# Build and run G0..G3; write a CSV of results only (for plotting).
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-"$APP_DIR/results_scalability.txt"}
LOG=${LOG:-"$APP_DIR/results_scalability.build.log"}

cd "$APP_DIR"
: > "$LOG"
printf 'bench,domains,fabrics,empty_cycles,net_cycles\n' > "$OUT"

# domains / fabrics for each variant (source hub + N domains)
domains_of() {
	case "$1" in
		G0) echo 1 ;;
		G1) echo 2 ;;
		G2) echo 4 ;;
		G3) echo 8 ;;
		*) echo 0 ;;
	esac
}

run_one() {
	bench=$1
	domains=$(domains_of "$bench")
	fabrics=$((domains + 1))

	echo "===== building $bench =====" | tee -a "$LOG"
	sed -i "s/^BENCH ?=.*/BENCH ?= $bench/" Makefile.uk
	kraft -v build --no-progress --fast --compartmentalize --no-fetch --no-prepare \
		2>&1 | tee -a "$LOG" >/dev/null

	echo "===== running $bench =====" | tee -a "$LOG"
	set +e
	run_log=$(mktemp)
	./run.sh >"$run_log" 2>&1
	rc=$?
	set -e
	cat "$run_log" >> "$LOG"

	empty=$(grep -E '^empty_overhead:' "$run_log" | tail -1 | awk '{print $2}')
	net=$(grep -E "^${bench}_" "$run_log" | tail -1 | awk '{print $2}')
	rm -f "$run_log"

	if [ -z "${empty:-}" ] || [ -z "${net:-}" ]; then
		echo "[run_all] $bench: failed to parse cycles (rc=$rc)" | tee -a "$LOG" >&2
		return 1
	fi

	printf '%s,%s,%s,%s,%s\n' "$bench" "$domains" "$fabrics" "$empty" "$net" | tee -a "$OUT"

	if [ "$rc" -ne 0 ] && [ "$rc" -ne 83 ]; then
		echo "[run_all] $bench: qemu/timeout rc=$rc" | tee -a "$LOG" >&2
	fi
}

for g in G0 G1 G2 G3; do
	run_one "$g"
done

echo "data: $OUT"
echo "build/run log: $LOG"
