#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

python3 scripts/export_golden.py

cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --parallel
ctest --test-dir build --output-on-failure

CLI="$ROOT/build/crankle"
"$CLI" version | grep -q "0.2"
"$CLI" pack --input tests/golden/sample_small.f32 --shape 4 -o /tmp/parity.cran
"$CLI" verify /tmp/parity.cran
"$CLI" unpack --input /tmp/parity.cran -o /tmp/parity_out.f32
"$CLI" stats /tmp/parity.cran | grep -q beta1

echo "parity ok — golden + ctest + CLI pipeline"
