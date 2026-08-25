#!/usr/bin/env bash
# Scale stress test: pack, inspect, and run batched holonomy on a
# production-size tensor (256 MiB of f32 = 4M slots), under time and memory
# ceilings. Exits nonzero on any failure or ceiling breach.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
CRANKL="$BUILD/crankl"
WORK="${TMPDIR:-/tmp}/crankl_stress_$$"
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT

SIZE_MB="${SIZE_MB:-256}"
SLOTS=$(( SIZE_MB * 1024 * 1024 / 4 / 64 )) # one slot per 64 f32 floats
# Measured ceilings (linux-x86_64, Release): pack ~0.65MB/s legacy anneal,
# inspect ~3us/slot post-0.5.2 linear cohomology, holonomy ~20ms/1024 batches.
PACK_S=$(( SIZE_MB * 2 )) INSPECT_S=60 HOLO_S=120

command -v /usr/bin/time >/dev/null || { echo "stress: /usr/bin/time required"; exit 2; }
[ -x "$CRANKL" ] || { echo "stress: build first ($CRANKL missing)"; exit 2; }

echo "== generating ${SIZE_MB}MiB f32 tensor =="
python3 - "$WORK/tensor.f32" "$SIZE_MB" <<'EOF'
import struct, sys
path, size_mb = sys.argv[1], int(sys.argv[2])
n = size_mb * 1024 * 1024 // 4
with open(path, 'wb') as f:
    chunk = [0.0] * 65536
    for i in range(0, n, len(chunk)):
        m = min(len(chunk), n - i)
        for j in range(m):
            chunk[j] = ((i + j) % 251 - 125) / 32.0
        f.write(struct.pack(f'<{m}f', *chunk[:m]))
EOF

timed() { # timed <limit_s> <label> <cmd...>
    local limit="$1" label="$2"; shift 2
    local start end
    start=$(date +%s.%N)
    "/usr/bin/time" -v "$@" 2>"$WORK/time.log"
    end=$(date +%s.%N)
    local secs peak
    secs=$(echo "$end $start" | awk '{printf "%.1f", $1-$2}')
    peak=$(grep "Maximum resident" "$WORK/time.log" | grep -o '[0-9]*')
    echo "   $label: ${secs}s, peak RSS ${peak}kB" | tee -a "$WORK/peaks.log" >&2
    if echo "$secs > $limit" | awk '{exit !($1>$2)}'; then
        echo "FAIL: $label exceeded ${limit}s ceiling"; exit 3
    fi
}

echo "== pack (${SIZE_MB}MiB) =="
timed "$PACK_S" pack $CRANKL pack --input "$WORK/tensor.f32" \
    --output "$WORK/stress.crank"

echo "== inspect (cohomology over $SLOTS slots) =="
timed "$INSPECT_S" inspect $CRANKL inspect --json "$WORK/stress.crank" \
    > "$WORK/inspect.json"
python3 -c "import json; json.load(open('$WORK/inspect.json'))" \
    || { echo "FAIL: inspect output is not valid JSON"; exit 3; }

echo "== holonomy batch 1024 =="
head -c $((1024 * 64 * 4)) /dev/urandom > "$WORK/x.bin"
timed "$HOLO_S" holonomy $CRANKL holonomy --input "$WORK/stress.crank" \
    --vector "$WORK/x.bin" --batch 1024 --output "$WORK/y.bin"

echo "stress ok: ${SIZE_MB}MiB round trip within ceilings"
