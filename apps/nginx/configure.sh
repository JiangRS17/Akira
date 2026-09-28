#!/bin/bash
# Apply an Nginx fabric preset and optionally rebuild.
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$APP_DIR/../.." && pwd)"
CONFIGS_DIR="$APP_DIR/configs"
BUILD_SCRIPT="$REPO_ROOT/unikraft/fabric-support/build_fabric.sh"

usage() {
	cat <<EOF
Usage: $0 <preset> [--build]

矩阵画法（见 benchmark/matrix.yaml；backend=ept，布局同原 MPK）:
  n01-1d-mpk-none         1D  monolith(nginx+newlib+uksched+lwip)  harden:∅
  n02-2d-all-harden       2D  nginx+newlib | uksched+lwip          harden:全部
  n03-3d-none             3D  nginx+newlib | uksched | lwip        harden:∅
  n04-3d-nginx-harden     3D  nginx+newlib | uksched | lwip        harden:nginx
  n05-2d-nginx-harden     2D  nginx+newlib | uksched+lwip          harden:nginx
  n06-2d-3harden          2D  nginx+newlib | uksched+lwip          harden:nginx,uksched,lwip
  n07-2d-all-harden       2D  nginx+newlib | uksched+lwip          harden:全部（同n02复测）

Options:
  --build       Run build_fabric.sh after applying fabric.yaml

Examples:
  $0 n01-1d-mpk-none --build
  ./benchmark/run_sequential.sh
EOF
}

list_presets() {
	echo "Available presets:"
	for f in "$CONFIGS_DIR"/n*.yaml; do
		[[ -f "$f" ]] || continue
		echo "  $(basename "$f" .yaml)"
	done
}

if [[ $# -lt 1 ]]; then
	usage
	list_presets
	exit 1
fi

PRESET="$1"
shift
DO_BUILD=0
for arg in "$@"; do
	case "$arg" in
	--build) DO_BUILD=1 ;;
	-h|--help) usage; exit 0 ;;
	*) echo "Unknown option: $arg" >&2; usage; exit 1 ;;
	esac
done

SRC="$CONFIGS_DIR/${PRESET}.yaml"
if [[ ! -f "$SRC" ]]; then
	echo "Unknown preset: $PRESET" >&2
	list_presets
	exit 1
fi

cp "$SRC" "$APP_DIR/fabric.yaml"
echo "Applied preset: $PRESET -> fabric.yaml"
echo "  isolation: $(grep -E '^\s+backend:' "$APP_DIR/fabric.yaml" | awk '{print $2}')"
echo "  fabrics:   $(grep -cE '^\s+fabric-[0-9]+:' "$APP_DIR/fabric.yaml" || echo 0)"

# Keep kraft VMEPT compartment count in sync with fabric domains.
# Monolith (n01) must be COMP_COUNT=1 — dual-QEMU + 1 fabric deadlocks.
python3 "$APP_DIR/sync_kraft_compartments.py" "$APP_DIR"

if [[ "$DO_BUILD" -eq 1 ]]; then
	echo ""
	echo "=== build_fabric.sh apps/nginx ==="
	sh "$BUILD_SCRIPT" "$APP_DIR"
	echo ""
	echo "Build complete. Run benchmark with:"
	echo "  ./benchmark/run_one.sh $PRESET"
fi
