#!/usr/bin/env bash
# Benchmark crankle finetune: reconstruction + optional holonomy task loss.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$ROOT/build/crankle"
IN="$ROOT/tests/golden/sample_small.f32"
BASE="/tmp/crankle_bench"

cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --parallel

"$CLI" pack --input "$IN" -o "${BASE}.cran"
"$CLI" finetune --input "${BASE}.cran" --target "$IN" --steps 50 --lr 0.04 \
    -o "${BASE}_ft.cran" | tee "${BASE}_ft.log"
grep -q recon_after "${BASE}_ft.log"

echo "bench_finetune ok"
