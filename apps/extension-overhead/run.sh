#!/bin/sh
# Run one extension-overhead binary. Needs -cpu host for guest PKU.
#
# When stdout is a pipe (run_all.sh | tee):
#   - < /dev/null avoids SIGTTIN (QEMU otherwise stops in state "T")
#   - stdbuf -oL keeps serial output line-buffered
#   - isa-debug-exit lets ukplat_terminate actually quit QEMU (port 0x501)
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KERNEL="$APP_DIR/build/extension-overhead_kvm-x86_64"
MEM=${MEM:-2048}
TIMEOUT=${TIMEOUT:-120}

if [ ! -x "$KERNEL" ]; then
	echo "missing $KERNEL — build first" >&2
	exit 1
fi

exec stdbuf -oL -eL timeout "$TIMEOUT" qemu-system-x86_64 \
	-enable-kvm -cpu host -nographic -m "$MEM" \
	-device isa-debug-exit,iobase=0x501,iosize=0x2 \
	-kernel "$KERNEL" -append "console=ttyS0" \
	< /dev/null
