#!/usr/bin/env python3
"""Export golden reference vectors for C++ parity tests."""
from __future__ import annotations

import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / "tests" / "golden"
GOLDEN.mkdir(parents=True, exist_ok=True)


def clifford_resonance_scalar(a: float, b: float) -> float:
    return (a * b) / max(1e-12, (abs(a) + abs(b)))


def main() -> None:
    sample = [float(i) * 0.1 for i in range(16)]
    (GOLDEN / "sample.f32").write_bytes(struct.pack(f"{len(sample)}f", *sample))

    clifford = {
        "pairs": [
            {"a_scalar": 1.0, "b_scalar": 1.0, "resonance_approx": clifford_resonance_scalar(1.0, 1.0)},
            {"a_scalar": 1.0, "b_scalar": 0.0, "resonance_approx": clifford_resonance_scalar(1.0, 0.0)},
        ]
    }
    (GOLDEN / "clifford_ref.json").write_text(json.dumps(clifford, indent=2))

    manifest = {
        "version": "0.1.0",
        "tolerance": 1e-3,
        "files": ["sample.f32", "clifford_ref.json"],
    }
    (GOLDEN / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print(f"exported golden to {GOLDEN}")


if __name__ == "__main__":
    main()
