#!/bin/bash
# Deprecated: use benchmark/run_matrix.sh
exec "$(cd "$(dirname "$0")/.." && pwd)/benchmark/run_matrix.sh" "$@"
