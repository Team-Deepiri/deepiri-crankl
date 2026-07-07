#!/usr/bin/env bash
# End-to-end CLI integration: pack → turn → peel → diff → holonomy
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$ROOT/build/crankle"
IN="$ROOT/tests/golden/sample_small.f32"
BASE="/tmp/crankle_e2e"

cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --parallel

"$CLI" pack --input "$IN" --shape 4 -o "${BASE}.cran"
"$CLI" turn --input "${BASE}.cran" --steps 10 --lr 0.05 -o "${BASE}_turned.cran"
"$CLI" peel --input "${BASE}_turned.cran" --layers 1 -o "${BASE}_peeled.cran"
"$CLI" diff "${BASE}.cran" "${BASE}_turned.cran" | grep -q slots_changed
"$CLI" holonomy --input "${BASE}_peeled.cran" --vector "$IN" -o "${BASE}_out.f32"
"$CLI" stats "${BASE}_peeled.cran" | grep -q n_slots

echo "e2e ok"
