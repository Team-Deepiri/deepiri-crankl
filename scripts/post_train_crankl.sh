#!/usr/bin/env bash
# Post-training hook: pack float weights → finetune crank archive → emit manifest.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$ROOT/build/crankl"

INPUT="${1:?usage: post_train_crankl.sh weights.f32 output.cran [manifest.json]}"
OUTPUT="${2:?missing output .cran path}"
MANIFEST="${3:-}"

cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --parallel

TMP="${OUTPUT%.cran}_packed.cran"
"$CLI" pack --input "$INPUT" -o "$TMP"
if [[ -n "$MANIFEST" ]]; then
    "$CLI" finetune --input "$TMP" --target "$INPUT" --steps 100 --lr 0.03 -o "$OUTPUT" \
        | tee /tmp/crankl_post_train.log
    python3 - "$MANIFEST" /tmp/crankl_post_train.log <<'PY'
import json, re, sys
manifest, log_path = sys.argv[1], sys.argv[2]
metrics = {}
for line in open(log_path):
    m = re.match(r"(\w+)=(\S+)", line.strip())
    if m:
        k, v = m.group(1), m.group(2)
        try:
            metrics[k] = float(v)
        except ValueError:
            metrics[k] = v
with open(manifest, "w") as f:
    json.dump({"tool": "crankl", "stage": "post_train", "metrics": metrics}, f, indent=2)
PY
else
    "$CLI" finetune --input "$TMP" --target "$INPUT" --steps 100 --lr 0.03 -o "$OUTPUT"
fi
echo "post_train ok: $OUTPUT"
