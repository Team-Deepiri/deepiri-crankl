#!/usr/bin/env bash
# End-to-end CLI integration: pack → turn → peel → inspect → compare → pipeline → holonomy
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$ROOT/build/crankle"
IN="$ROOT/tests/golden/sample_small.f32"
BASE="/tmp/crankle_e2e"

cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --parallel

"$CLI" pack --input "$IN" --shape 4 -o "${BASE}.cran"
"$CLI" turn --input "${BASE}.cran" --steps 10 --lr 0.05 -o "${BASE}_turned.cran"
"$CLI" finetune --input "${BASE}_turned.cran" --target "$IN" --steps 20 --lr 0.04 \
    -o "${BASE}_finetuned.cran" | grep -q recon_after
"$CLI" peel --input "${BASE}_finetuned.cran" --layers 1 -o "${BASE}_peeled.cran"
"$CLI" diff "${BASE}.cran" "${BASE}_turned.cran" | grep -q slots_changed
"$CLI" inspect "${BASE}_peeled.cran" --json | grep -q trit_density
"$CLI" compare "${BASE}.cran" "${BASE}_turned.cran" --json | grep -q clifford_resonance
"$CLI" holonomy --input "${BASE}_peeled.cran" --vector "$IN" -o "${BASE}_out.f32"
"$CLI" stats "${BASE}_peeled.cran" | grep -q n_slots
"$CLI" pipeline --input "$IN" --steps 6 --lr 0.04 -o "${BASE}_pipeline.cran" --manifest "${BASE}_manifest.json" | grep -q pipeline_output
test -s "${BASE}_manifest.json"
grep -q '"metrics"' "${BASE}_manifest.json"

echo "e2e ok"
