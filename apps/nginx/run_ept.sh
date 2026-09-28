#!/bin/bash
# Run nginx unikernel with FlexOS VM/EPT (N QEMU guests + myshmem).
#
# Compartment 0 (app/nginx) gets networking + 9p rootfs.
# Compartment 1..N-1 share RPC/heap/data_shared via myshmem.
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APP_DIR"

QEMU="${QEMU:-/root/qemu-system-ept}"
KERNEL_BASE="${KERNEL_BASE:-$APP_DIR/build/nginx_kvm-x86_64}"
COMP0="${COMP0:-${KERNEL_BASE}.comp0}"
COMP1="${COMP1:-${KERNEL_BASE}.comp1}"
COMP2="${COMP2:-${KERNEL_BASE}.comp2}"
DBG0="${DBG0:-${KERNEL_BASE}.dbg.comp0}"

ROOTFS="${ROOTFS:-$APP_DIR/../../libs/nginx/nginx-rootfs-example}"
HOST_PORT="${HOST_PORT:-8080}"
MEM="${MEM:-2G}"

SHM_PREFIX="${SHM_PREFIX:-/nginx-ept}"
RPC_SHM="${RPC_SHM:-${SHM_PREFIX}-rpc}"
HEAP_SHM="${HEAP_SHM:-${SHM_PREFIX}-heap}"
DATA_SHM="${DATA_SHM:-${SHM_PREFIX}-data}"

# FlexOS hardcoded GPAs (see flexos/impl/vmept.h)
RPC_PADDR=0x800000000
RPC_SIZE=0x100000
HEAP_PADDR=0x4000000000
HEAP_SIZE=0x8000000

PIDDIR="${PIDDIR:-/tmp/nginx-ept}"
LOG0="${LOG0:-$APP_DIR/qemu-ept-comp0.log}"
LOG1="${LOG1:-$APP_DIR/qemu-ept-comp1.log}"
LOG2="${LOG2:-$APP_DIR/qemu-ept-comp2.log}"

NETDEV_ARGS="${NETDEV_ARGS:-netdev.ipv4_addr=10.0.2.15 netdev.ipv4_gw_addr=10.0.2.2 netdev.ipv4_subnet_mask=255.255.255.0}"
QEMU_APPEND="${QEMU_APPEND:-console=ttyS0 vfs.rootdev=fs0 ${NETDEV_ARGS} --}"

usage() {
	cat <<EOF
Usage: \$0 [start|stop|run|status|all]

  start   Launch comp0..compN-1 (myshmem RPC/heap/data_shared)
  stop    Stop QEMU guests and unlink shm
  run     curl guest HTTP
  status  Show pids / images
  all     start, wait, curl, stop

Environment: QEMU, COMP0, COMP1, COMP2, ROOTFS, HOST_PORT, SHM_PREFIX
EOF
}

need_file() {
	if [[ ! -f "$1" ]]; then
		echo "Missing: $1" >&2
		echo "Build first: ./configure.sh n07-2d-all-harden --build" >&2
		echo "  (or: sh ../../unikraft/fabric-support/build_fabric.sh $APP_DIR)" >&2
		exit 1
	fi
}

data_shared_size() {
	local dbg="$1"
	local sz
	sz=$(readelf -SW "$dbg" 2>/dev/null | awk '/\.data_shared/ {print $7; exit}')
	if [[ -z "$sz" ]]; then
		echo "0x3000"
		return
	fi
	printf '0x%s\n' "$sz"
}

count_comps() {
	local n=0
	local i
	for i in 0 1 2 3 4 5 6 7; do
		if [[ -f "${KERNEL_BASE}.comp${i}" ]]; then
			n=$((i + 1))
		else
			break
		fi
	done
	echo "$n"
}

stop_guests() {
	mkdir -p "$PIDDIR"
	local f
	for f in "$PIDDIR"/comp*.pid; do
		[[ -f "$f" ]] || continue
		kill "$(cat "$f")" 2>/dev/null || true
		rm -f "$f"
	done
	pkill -f "qemu-system-ept.*nginx_kvm-x86_64" 2>/dev/null || true
	pkill -f "${QEMU}.*nginx_kvm-x86_64" 2>/dev/null || true
	rm -f "/dev/shm/${RPC_SHM#/}" "/dev/shm/${HEAP_SHM#/}" "/dev/shm/${DATA_SHM#/}" 2>/dev/null || true
}

wait_for_http() {
	local i
	for i in $(seq 1 60); do
		if curl -sf "http://127.0.0.1:${HOST_PORT}/" >/dev/null 2>&1; then
			return 0
		fi
		sleep 1
	done
	return 1
}

common_myshmem_args() {
	local data_sz="$1"
	echo -device "myshmem,file=${RPC_SHM},paddr=${RPC_PADDR},size=${RPC_SIZE}" \
		-device "myshmem,file=${HEAP_SHM},paddr=${HEAP_PADDR},size=${HEAP_SIZE}" \
		-device "myshmem,file=${DATA_SHM},paddr=0x105000,size=${data_sz}"
}

start_one_peer() {
	local idx="$1"
	local data_sz="$2"
	local kernel="${KERNEL_BASE}.comp${idx}"
	local log="$APP_DIR/qemu-ept-comp${idx}.log"
	need_file "$kernel"
	# shellcheck disable=SC2046
	"$QEMU" \
		-machine pc,accel=kvm \
		-cpu host \
		-enable-kvm \
		-m "$MEM" \
		-nographic \
		-device isa-debug-exit \
		$(common_myshmem_args "$data_sz") \
		-kernel "$kernel" \
		-append "console=ttyS0 --" \
		> "$log" 2>&1 &
	echo $! >"$PIDDIR/comp${idx}.pid"
	echo "comp${idx} pid $(cat "$PIDDIR/comp${idx}.pid") log=$log"
}

start_guests() {
	need_file "$QEMU"
	need_file "$COMP0"
	[[ -d "$ROOTFS" ]] || {
		echo "Missing rootfs: $ROOTFS" >&2
		exit 1
	}

	local ncomp
	ncomp=$(count_comps)
	if [[ "$ncomp" -lt 1 ]]; then
		echo "No ${KERNEL_BASE}.comp0 found" >&2
		exit 1
	fi

	stop_guests
	mkdir -p "$PIDDIR"

	local data_sz
	if [[ -f "$DBG0" ]]; then
		data_sz=$(data_shared_size "$DBG0")
	else
		data_sz=0x4000
	fi
	echo "data_shared size: $data_sz"
	echo "EPT guests: $ncomp"

	# Comp0 = app (nginx) — networking + 9p
	# shellcheck disable=SC2046
	"$QEMU" \
		-machine pc,accel=kvm \
		-cpu host \
		-enable-kvm \
		-m "$MEM" \
		-nographic \
		-device isa-debug-exit \
		$(common_myshmem_args "$data_sz") \
		-kernel "$COMP0" \
		-append "$QEMU_APPEND" \
		-fsdev "local,id=fsdev0,path=${ROOTFS},security_model=none" \
		-device "virtio-9p-pci,fsdev=fsdev0,mount_tag=fs0" \
		-netdev "user,id=net0,hostfwd=tcp::${HOST_PORT}-:80" \
		-device virtio-net-pci,netdev=net0 \
		> "$LOG0" 2>&1 &
	echo $! >"$PIDDIR/comp0.pid"
	echo "comp0 pid $(cat "$PIDDIR/comp0.pid") log=$LOG0"

	local i
	for ((i = 1; i < ncomp; i++)); do
		sleep 0.5
		start_one_peer "$i" "$data_sz"
	done

	echo "Waiting for HTTP on :${HOST_PORT} (up to 60s)..."
	if ! wait_for_http; then
		echo "HTTP not reachable" >&2
		local j
		for ((j = 0; j < ncomp; j++)); do
			echo "===== comp${j} log (tail) =====" >&2
			tail -80 "$APP_DIR/qemu-ept-comp${j}.log" >&2 || true
		done
		exit 1
	fi
	echo "nginx EPT is up: http://127.0.0.1:${HOST_PORT}/"
}

show_status() {
	local ncomp
	ncomp=$(count_comps)
	local i
	for ((i = 0; i < ncomp || i < 3; i++)); do
		local f="${KERNEL_BASE}.comp${i}"
		echo "COMP${i}=$f $( [[ -f $f ]] && echo OK || echo MISSING )"
	done
	echo "QEMU=$QEMU"
	for f in "$PIDDIR"/comp*.pid; do
		[[ -f "$f" ]] || continue
		echo "$(basename "$f" .pid) pid=$(cat "$f")"
	done
}

case "${1:-start}" in
start) start_guests ;;
stop) stop_guests ;;
run) curl -v "http://127.0.0.1:${HOST_PORT}/" ;;
status) show_status ;;
all)
	start_guests
	curl -v "http://127.0.0.1:${HOST_PORT}/"
	stop_guests
	;;
-h|--help) usage ;;
*) usage; exit 1 ;;
esac
