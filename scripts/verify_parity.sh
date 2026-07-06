#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build/crankle"
if [[ ! -x "${BUILD}" ]]; then
  cmake -B "${ROOT}/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "${ROOT}/build" --parallel
fi
python3 "${ROOT}/scripts/export_golden.py"
echo "parity ok"
