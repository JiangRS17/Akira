#!/bin/bash
# Apply a Redis fabric preset and optionally rebuild.
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$APP_DIR/../.." && pwd)"
CONFIGS_DIR="$APP_DIR/configs"
BUILD_SCRIPT="$REPO_ROOT/unikraft/fabric-support/build_fabric.sh"

usage() {
	cat <<EOF
Usage: $0 <preset> [--build]

矩阵配置（见 benchmark/matrix.yaml）:
  r01-1d-mpk-none         1 域 MPK，无加固
  r02-2d-all-harden       2 域，4 组件全加固
  r03-3d-none             3 域，无加固
  r04-3d-redis-harden     3 域，仅 Redis 加固
  r05-2d-redis-harden     2 域，仅 Redis 加固
  r06-2d-3harden          2 域，Redis+uksched+lwip 加固
  r07-2d-all-harden       2 域，4 组件全加固

Options:
  --build       Run build_fabric.sh after applying fabric.yaml

Examples:
  $0 c01-monolith-mpk --build
  ./benchmark/run_matrix.sh
EOF
}

list_presets() {
	echo "Available presets:"
	for f in "$CONFIGS_DIR"/r*.yaml; do
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

if [[ "$DO_BUILD" -eq 1 ]]; then
	echo ""
	echo "=== build_fabric.sh apps/redis ==="
	sh "$BUILD_SCRIPT" "$APP_DIR"
	echo ""
	echo "Build complete. Run benchmark with:"
	echo "  ./benchmark/run_matrix.sh $PRESET"
fi
