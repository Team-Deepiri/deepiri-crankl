#!/usr/bin/env bash
# End-to-end CLI integration: pack → turn → peel → inspect → compare → pipeline → holonomy
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$ROOT/build/crankl"
IN="$ROOT/tests/golden/sample_small.f32"
BASE="/tmp/crankl_e2e"

cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --parallel

"$CLI" pack --input "$IN" --shape 4 -o "${BASE}.crank"
"$CLI" turn --input "${BASE}.crank" --steps 10 --lr 0.05 -o "${BASE}_turned.crank"
"$CLI" finetune --input "${BASE}_turned.crank" --target "$IN" --steps 20 --lr 0.04 \
    -o "${BASE}_finetuned.crank" | grep -q recon_after
"$CLI" peel --input "${BASE}_finetuned.crank" --layers 1 -o "${BASE}_peeled.crank"
"$CLI" diff "${BASE}.crank" "${BASE}_turned.crank" | grep -q slots_changed
"$CLI" inspect "${BASE}_peeled.crank" --json | grep -q trit_density
"$CLI" compare "${BASE}.crank" "${BASE}_turned.crank" --json | grep -q clifford_resonance
"$CLI" holonomy --input "${BASE}_peeled.crank" --vector "$IN" -o "${BASE}_out.f32"
"$CLI" stats "${BASE}_peeled.crank" | grep -q n_slots
"$CLI" pipeline --input "$IN" --steps 6 --lr 0.04 -o "${BASE}_pipeline.crank" --manifest "${BASE}_manifest.json" | grep -q pipeline_output
test -s "${BASE}_manifest.json"
grep -q '"metrics"' "${BASE}_manifest.json"

echo "e2e ok"
