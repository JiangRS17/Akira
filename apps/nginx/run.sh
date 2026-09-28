#!/bin/bash
# Run nginx unikernel with 9p rootfs + host port forward (8080 -> guest :80)
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APP_DIR"

UNIKERNEL="${UNIKERNEL:-$APP_DIR/build/nginx_kvm-x86_64}"
ROOTFS="${ROOTFS:-$APP_DIR/../../libs/nginx/nginx-rootfs-example}"
HOST_PORT="${HOST_PORT:-8080}"
QEMU_PIDFILE="${QEMU_PIDFILE:-/tmp/nginx-qemu.pid}"
QEMU_LOG="${QEMU_LOG:-$APP_DIR/qemu.log}"

NETDEV_ARGS="${NETDEV_ARGS:-netdev.ipv4_addr=10.0.2.15 netdev.ipv4_gw_addr=10.0.2.2 netdev.ipv4_subnet_mask=255.255.255.0}"
# Kernel/lib params before "--"; nginx sees nothing after "--" unless you add app args.
QEMU_APPEND="${QEMU_APPEND:-console=ttyS0 vfs.rootdev=fs0 ${NETDEV_ARGS} --}"

usage() {
	cat <<EOF
Usage: $0 [start|stop|run|all]

  start   Launch unikernel (QEMU + 9p rootfs + virtio-net)
  stop    Stop QEMU
  run     curl guest HTTP (guest must be up)
  all     start, wait, curl, stop

Environment: UNIKERNEL, ROOTFS, HOST_PORT, QEMU_APPEND, NETDEV_ARGS

Example:
  $0 start
  curl http://127.0.0.1:${HOST_PORT}/
EOF
}

need_file() {
	if [[ ! -f "$1" ]]; then
		echo "Missing file: $1 (build first)" >&2
		exit 1
	fi
}

stop_guest() {
	if [[ -f "$QEMU_PIDFILE" ]]; then
		kill "$(cat "$QEMU_PIDFILE")" 2>/dev/null || true
		rm -f "$QEMU_PIDFILE"
	fi
	pkill -f "qemu-system-x86_64.*nginx_kvm-x86_64" 2>/dev/null || true
}

wait_for_http() {
	local i
	for i in $(seq 1 30); do
		if curl -sf "http://127.0.0.1:${HOST_PORT}/" >/dev/null 2>&1; then
			return 0
		fi
		sleep 1
	done
	return 1
}

start_guest() {
	need_file "$UNIKERNEL"
	[[ -d "$ROOTFS" ]] || {
		echo "Missing rootfs dir: $ROOTFS" >&2
		exit 1
	}

	stop_guest

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
		-netdev "user,id=net0,hostfwd=tcp::${HOST_PORT}-:80" \
		-device virtio-net-pci,netdev=net0 \
		> "$QEMU_LOG" 2>&1 &

	echo $! > "$QEMU_PIDFILE"
	echo "QEMU pid $(cat "$QEMU_PIDFILE"), log: $QEMU_LOG"
	echo "Waiting for HTTP on port ${HOST_PORT} (up to 30s)..."
	if ! wait_for_http; then
		echo "HTTP not reachable on :${HOST_PORT}" >&2
		tail -40 "$QEMU_LOG" >&2 || true
		exit 1
	fi
	echo "nginx is up: http://127.0.0.1:${HOST_PORT}/"
}

run_curl() {
	curl -v "http://127.0.0.1:${HOST_PORT}/"
}

case "${1:-start}" in
start) start_guest ;;
stop) stop_guest ;;
run) run_curl ;;
all)
	start_guest
	run_curl
	stop_guest
	;;
-h|--help) usage ;;
*) usage; exit 1 ;;
esac
