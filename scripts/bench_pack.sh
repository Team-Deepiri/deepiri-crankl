#!/usr/bin/env bash
# Benchmark crankl pack: CLI pack timing plus per-mode (legacy/staged/BO) LoRA-scale timings.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build"
cmake -B "${BUILD}" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "${BUILD}" --parallel >/dev/null

/usr/bin/time -f "pack_elapsed=%e" "${BUILD}/crankl" pack --input "${ROOT}/tests/golden/sample.f32" --shape 8 -o /tmp/bench.crank

CC_BIN="${CC:-cc}"
if "$CC_BIN" -O2 -I"${ROOT}/include" "${ROOT}/tools/bench_pack.c" -L"${BUILD}" -lcrankl \
    -Wl,-rpath,"${BUILD}" -o /tmp/crankl_bench_pack 2>/dev/null; then
    /tmp/crankl_bench_pack
else
    echo "bench_pack: mode bench skipped (no compiler or lib)"
fi
