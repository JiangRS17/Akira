#!/bin/bash
# Nginx benchmark for Akira-Fabric (based on asplos22-ae fig-06 apps/nginx/test.sh)
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APP_DIR"

UNIKERNEL="${UNIKERNEL:-$APP_DIR/build/nginx_kvm-x86_64}"
ROOTFS="${ROOTFS:-$APP_DIR/../../libs/nginx/nginx-rootfs-example}"
RESULTS="${RESULTS:-$APP_DIR/benchmark/results.csv}"
QEMU_PIDFILE="${QEMU_PIDFILE:-/tmp/nginx-benchmark-qemu.pid}"
QEMU_LOG="${QEMU_LOG:-$APP_DIR/benchmark/qemu.log}"

NETDEV_ARGS="${NETDEV_ARGS:-netdev.ipv4_addr=10.0.2.15 netdev.ipv4_gw_addr=10.0.2.2 netdev.ipv4_subnet_mask=255.255.255.0}"
QEMU_APPEND="${QEMU_APPEND:-console=ttyS0 vfs.rootdev=fs0 ${NETDEV_ARGS} --}"

# Match fig-06 apps/nginx/test.sh defaults
NUM_PARALLEL_CONNS="${NUM_PARALLEL_CONNS:-30}"
NUM_THREADS="${NUM_THREADS:-14}"
DURATION="${DURATION:-10s}"
ITERATIONS="${ITERATIONS:-5}"
BOOT_WARMUP_SLEEP="${BOOT_WARMUP_SLEEP:-4}"
CHUNK="${CHUNK:-0}"
HTTP_HOST="${HTTP_HOST:-127.0.0.1}"
HTTP_PORT="${HTTP_PORT:-8080}"
TASKID="${TASKID:-fabric-nginx-single-fabric}"
CONFIG_ID="${CONFIG_ID:-}"
DOMAINS="${DOMAINS:-}"
BACKEND="${BACKEND:-}"
SPLIT="${SPLIT:-}"
HARDENING="${HARDENING:-}"
RESULTS_EXTENDED="${RESULTS_EXTENDED:-0}"

# EPT: multi-conn wrk deadlocks the single RPC/select path; keep c=1 unless overridden.
if [[ "${BACKEND}" == "ept" ]]; then
	if [[ -z "${NUM_PARALLEL_CONNS_SET:-}" && "${NUM_PARALLEL_CONNS}" == "30" ]]; then
		NUM_PARALLEL_CONNS=1
	fi
	if [[ -z "${NUM_THREADS_SET:-}" && "${NUM_THREADS}" == "14" ]]; then
		NUM_THREADS=1
	fi
fi

usage() {
	cat <<EOF
Usage: $0 [start|stop|run|all]

  start   Launch unikernel with 9p rootfs + virtio-net + hostfwd :${HTTP_PORT}->:80
  stop    Stop benchmark QEMU instance
  run     Run wrk load test (guest must be up)
  all     start + run + stop

Environment: NUM_PARALLEL_CONNS, NUM_THREADS, DURATION, ITERATIONS,
             BOOT_WARMUP_SLEEP, RESULTS, TASKID, HTTP_HOST, HTTP_PORT,
             RESULTS_EXTENDED, CONFIG_ID, DOMAINS, BACKEND, SPLIT, HARDENING

Example:
  BACKEND=ept ./benchmark.sh all
  ./benchmark/run_one.sh n07-2d-all-harden
  curl http://${HTTP_HOST}:${HTTP_PORT}/
EOF
}

need_file() {
	[[ -f "$1" ]] || { echo "Missing file: $1" >&2; exit 1; }
}

is_ept() {
	[[ "${BACKEND}" == "ept" ]] || [[ -f "${UNIKERNEL}.comp0" ]]
}

stop_guest() {
	if is_ept; then
		HOST_PORT="$HTTP_PORT" bash "$APP_DIR/run_ept.sh" stop 2>/dev/null || true
	fi
	if [[ -f "$QEMU_PIDFILE" ]]; then
		kill "$(cat "$QEMU_PIDFILE")" 2>/dev/null || true
		rm -f "$QEMU_PIDFILE"
	fi
	pkill -f "qemu-system-x86_64.*nginx_kvm-x86_64" 2>/dev/null || true
	pkill -f "qemu-system-ept.*nginx_kvm-x86_64" 2>/dev/null || true
}

wait_for_http() {
	local i
	for i in $(seq 1 60); do
		if curl -m 2 -sf "http://${HTTP_HOST}:${HTTP_PORT}/" >/dev/null 2>&1; then
			return 0
		fi
		sleep 1
	done
	return 1
}

start_guest() {
	[[ -d "$ROOTFS" ]] || { echo "Missing rootfs: $ROOTFS" >&2; exit 1; }
	stop_guest
	mkdir -p "$(dirname "$RESULTS")"

	if is_ept; then
		need_file "${UNIKERNEL}.comp0"
		if [[ -f "${UNIKERNEL}.comp1" ]]; then
			echo "EPT mode: dual QEMU via run_ept.sh (base=$UNIKERNEL)"
		else
			echo "EPT mode: single QEMU monolith (base=$UNIKERNEL, no .comp1)"
		fi
		KERNEL_BASE="$UNIKERNEL" \
		COMP0="${UNIKERNEL}.comp0" \
		COMP1="${UNIKERNEL}.comp1" \
		DBG0="${UNIKERNEL}.dbg.comp0" \
		HOST_PORT="$HTTP_PORT" \
		ROOTFS="$ROOTFS" \
		bash "$APP_DIR/run_ept.sh" start
		sleep "$BOOT_WARMUP_SLEEP"
		echo "nginx EPT is up."
		return 0
	fi

	need_file "$UNIKERNEL"
	qemu-system-x86_64 \
		-machine pc,accel=kvm \
		-cpu host \
		-enable-kvm \
		-m 1024 \
		-nographic \
		-kernel "$UNIKERNEL" \
		-append "$QEMU_APPEND" \
		-fsdev "local,id=fsdev0,path=${ROOTFS},security_model=none" \
		-device "virtio-9p-pci,fsdev=fsdev0,mount_tag=fs0" \
		-netdev "user,id=net0,hostfwd=tcp::${HTTP_PORT}-:80" \
		-device virtio-net-pci,netdev=net0 \
		> "$QEMU_LOG" 2>&1 &

	echo $! > "$QEMU_PIDFILE"
	echo "QEMU pid $(cat "$QEMU_PIDFILE"), log: $QEMU_LOG"
	echo "Waiting for nginx (up to 60s)..."
	sleep "$BOOT_WARMUP_SLEEP"
	if ! wait_for_http; then
		echo "HTTP not reachable at http://${HTTP_HOST}:${HTTP_PORT}/" >&2
		tail -40 "$QEMU_LOG" >&2 || true
		exit 1
	fi
	echo "nginx is up."
}

parse_wrk_req_per_sec() {
	# fig-06: awk on wrk line "Requests/sec: 12345.67"
	awk 'BEGIN{a["Requests/sec:"]} ($1 in a) && ($2 ~ /[0-9]/){print $2}'
}

run_benchmark() {
	command -v wrk >/dev/null || {
		echo "Install wrk: apt install wrk" >&2
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

	for ((I = 1; I <= ITERATIONS; I++)); do
		echo "=== iteration=${I}/${ITERATIONS} wrk -t${NUM_THREADS} -c${NUM_PARALLEL_CONNS} -d${DURATION} ==="
		if is_ept && [[ "${EPT_RESTART_BETWEEN_ITERS:-1}" == "1" ]] && [[ "$I" -gt 1 ]]; then
			echo "EPT: restarting guests between iterations..."
			stop_guest
			start_guest
		fi
		if ! curl -m 2 -sf "http://${HTTP_HOST}:${HTTP_PORT}/" >/dev/null 2>&1; then
			echo "HTTP down before iteration ${I} — guest likely crashed/wedged" >&2
			tail -30 "$APP_DIR/qemu-ept-comp0.log" 2>/dev/null || true
			exit 1
		fi
		if [[ "$RESULTS_EXTENDED" == "1" ]]; then
			prefix="${TASKID},${CONFIG_ID},${DOMAINS},${BACKEND},${SPLIT},${HARDENING},${CHUNK},${I},REQ"
		else
			prefix="${TASKID},${CHUNK},${I},REQ"
		fi
		wrk_extra=()
		if is_ept; then
			wrk_extra+=(-H 'Connection: close')
		fi
		wrk \
			-t "$NUM_THREADS" \
			-c "$NUM_PARALLEL_CONNS" \
			-d "$DURATION" \
			"${wrk_extra[@]}" \
			"http://${HTTP_HOST}:${HTTP_PORT}/" | \
			parse_wrk_req_per_sec | \
			awk -v prefix="$prefix" '{ print prefix "," $0 }' >> "$RESULTS"
	done

	echo "Results written to: $RESULTS"
	echo "--- summary (avg requests/sec) ---"
	if [[ "$RESULTS_EXTENDED" == "1" ]]; then
		awk -F, 'NR>1 && $9=="REQ" {sum+=$10; n++} END {if (n>0) printf "task=%s avg=%.2f req/s (n=%d iterations)\n", "'"$TASKID"'", sum/n, n}' "$RESULTS"
	else
		awk -F, 'NR>1 && $4=="REQ" {sum+=$5; n++} END {if (n>0) printf "task=%s avg=%.2f req/s (n=%d iterations)\n", "'"$TASKID"'", sum/n, n}' "$RESULTS"
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
