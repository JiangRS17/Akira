#!/bin/sh
# Run soft-bus grid scalability. Needs -cpu host for guest PKU.
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KERNEL="$APP_DIR/build/scalability_kvm-x86_64"
MEM=${MEM:-8192}
TIMEOUT=${TIMEOUT:-1800}

if [ ! -x "$KERNEL" ]; then
	echo "missing $KERNEL — build first" >&2
	exit 1
fi

exec stdbuf -oL -eL timeout "$TIMEOUT" qemu-system-x86_64 \
	-enable-kvm -cpu host -nographic -m "$MEM" \
	-device isa-debug-exit,iobase=0x501,iosize=0x2 \
	-kernel "$KERNEL" -append "console=ttyS0" \
	< /dev/null
