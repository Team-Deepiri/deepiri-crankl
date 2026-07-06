#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
cmake -B "${BUILD}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD}" --parallel
/usr/bin/time -f "pack_elapsed=%e" "${BUILD}/crankle" pack --input "${ROOT}/tests/golden/sample.f32" --shape 8 -o /tmp/bench.cran
