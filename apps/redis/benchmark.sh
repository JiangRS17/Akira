#!/bin/bash
# Redis benchmark for Unikraft (based on asplos22-ae fig-06 redis test.sh)
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APP_DIR"

UNIKERNEL="${UNIKERNEL:-$APP_DIR/build/redis_kvm-x86_64}"
RESULTS="${RESULTS:-$APP_DIR/benchmark/results.csv}"
QEMU_PIDFILE="${QEMU_PIDFILE:-/tmp/redis-benchmark-qemu.pid}"

# QEMU user netdev defaults (required for hostfwd tcp::6379-:6379)
NETDEV_ARGS="${NETDEV_ARGS:-netdev.ipv4_addr=10.0.2.15 netdev.ipv4_gw_addr=10.0.2.2 netdev.ipv4_subnet_mask=255.255.255.0}"

# Redis default bind is 127.0.0.1; open virtio-net for hostfwd
REDIS_ARGS="${REDIS_ARGS:---bind 0.0.0.0 --protected-mode no}"

# Match fig-06 defaults (apps/redis/test.sh)
NUM_REQUESTS="${NUM_REQUESTS:-100000}"
NUM_CLIENTS="${NUM_CLIENTS:-30}"
PIPELINE="${PIPELINE:-16}"
CHUNKS="${CHUNKS:-5 50 500}"
ITERATIONS="${ITERATIONS:-3}"
BOOT_SLEEP="${BOOT_SLEEP:-5}"
REDIS_HOST="${REDIS_HOST:-127.0.0.1}"
REDIS_PORT="${REDIS_PORT:-6379}"
TASKID="${TASKID:-fabric-redis-single-fabric}"
CONFIG_ID="${CONFIG_ID:-}"
DOMAINS="${DOMAINS:-}"
BACKEND="${BACKEND:-}"
SPLIT="${SPLIT:-}"
HARDENING="${HARDENING:-}"
RESULTS_EXTENDED="${RESULTS_EXTENDED:-0}"

usage() {
	cat <<EOF
Usage: $0 [start|stop|run|all]

  start   Launch unikernel with virtio-net + host port forward
  stop    Stop the benchmark QEMU instance
  run     Run redis-benchmark (guest must be up)
  all     start + run + stop

Environment: NUM_REQUESTS, NUM_CLIENTS, PIPELINE, CHUNKS, ITERATIONS,
             RESULTS, TASKID, BOOT_SLEEP, REDIS_HOST, REDIS_PORT

Example:
  ./benchmark.sh all
  redis-cli -h ${REDIS_HOST} -p ${REDIS_PORT} set mykey hello
  redis-cli -h ${REDIS_HOST} -p ${REDIS_PORT} get mykey
EOF
}

need_file() {
	if [[ ! -f "$1" ]]; then
		echo "Missing file: $1" >&2
		exit 1
	fi
}

stop_guest() {
	if [[ -f "$QEMU_PIDFILE" ]]; then
		kill "$(cat "$QEMU_PIDFILE")" 2>/dev/null || true
		rm -f "$QEMU_PIDFILE"
	fi
	pkill -f "qemu-system-x86_64.*redis_kvm-x86_64" 2>/dev/null || true
}

wait_for_redis() {
	local i
	for i in $(seq 1 30); do
		if redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" ping 2>/dev/null | grep -q PONG; then
			return 0
		fi
		sleep 1
	done
	return 1
}

start_guest() {
	need_file "$UNIKERNEL"
	stop_guest

	QEMU_APPEND="console=ttyS0 ${NETDEV_ARGS} -- ${REDIS_ARGS}"

	qemu-system-x86_64 \
		-machine pc,accel=kvm \
		-cpu host \
		-enable-kvm \
		-m 1024 \
		-nographic \
		-kernel "$UNIKERNEL" \
		-append "$QEMU_APPEND" \
		-netdev "user,id=net0,hostfwd=tcp::${REDIS_PORT}-:6379" \
		-device virtio-net-pci,netdev=net0 \
		> "$APP_DIR/benchmark/qemu.log" 2>&1 &

	echo $! > "$QEMU_PIDFILE"
	echo "QEMU pid $(cat "$QEMU_PIDFILE"), log: $APP_DIR/benchmark/qemu.log"
	echo "Waiting for Redis (up to 30s)..."
	if ! wait_for_redis; then
		echo "Redis not reachable at ${REDIS_HOST}:${REDIS_PORT}" >&2
		tail -30 "$APP_DIR/benchmark/qemu.log" >&2 || true
		stop_guest
		exit 1
	fi
	echo "Redis is up (PONG)."
}

run_benchmark() {
	command -v redis-benchmark >/dev/null || {
		echo "Install redis-tools: apt install redis-tools" >&2
		exit 1
	}

	mkdir -p "$(dirname "$RESULTS")"
	if [[ ! -f "$RESULTS" ]]; then
		if [[ "$RESULTS_EXTENDED" == "1" ]]; then
			echo "TASKID,CONFIG_ID,DOMAINS,BACKEND,SPLIT,HARDENING,CHUNK,ITERATION,METHOD,VALUE" > "$RESULTS"
		else
			echo "TASKID,CHUNK,ITERATION,METHOD,VALUE" > "$RESULTS"
		fi
	fi

	for CHUNK in $CHUNKS; do
		for ((I = 1; I <= ITERATIONS; I++)); do
			echo "=== chunk=${CHUNK} iteration=${I}/${ITERATIONS} ==="
			if [[ "$RESULTS_EXTENDED" == "1" ]]; then
				prefix="${TASKID},${CONFIG_ID},${DOMAINS},${BACKEND},${SPLIT},${HARDENING},${CHUNK},${I}"
			else
				prefix="${TASKID},${CHUNK},${I}"
			fi
			redis-benchmark \
				-h "$REDIS_HOST" -p "$REDIS_PORT" \
				-n "$NUM_REQUESTS" \
				-c "$NUM_CLIENTS" \
				-P "$PIPELINE" \
				-k 1 \
				-t get,set \
				-d "$CHUNK" \
				--csv -q | \
				awk -v prefix="$prefix" '{ print prefix "," $0 }' >> "$RESULTS"
			sed -i 's/"//g' "$RESULTS"
		done
	done

	echo "Results written to: $RESULTS"
	echo "--- summary (avg requests/sec by chunk & method) ---"
	if [[ "$RESULTS_EXTENDED" == "1" ]]; then
		awk -F, 'NR>1 {key=$7","$9; sum[key]+=$10; n[key]++} END {for (k in sum) {split(k,a,","); printf "chunk=%s method=%s avg=%.2f req/s (n=%d)\n", a[1], a[2], sum[k]/n[k], n[k]}}' "$RESULTS" | sort -t= -k2,2n -k4,4
	else
		awk -F, 'NR>1 {key=$2","$4; sum[key]+=$5; n[key]++} END {for (k in sum) {split(k,a,","); printf "chunk=%s method=%s avg=%.2f req/s (n=%d)\n", a[1], a[2], sum[k]/n[k], n[k]}}' "$RESULTS" | sort -t= -k2,2n -k4,4
	fi
}

case "${1:-all}" in
start) start_guest ;;
stop) stop_guest ;;
run) run_benchmark ;;
all)
	start_guest
	run_benchmark
	stop_guest
	;;
-h|--help) usage ;;
*) usage; exit 1 ;;
esac
