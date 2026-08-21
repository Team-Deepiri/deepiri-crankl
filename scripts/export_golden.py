#!/usr/bin/env python3
"""Export golden references and generate C++ parity headers from notebook math."""
from __future__ import annotations

import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / "tests" / "golden"
GOLDEN.mkdir(parents=True, exist_ok=True)


def mv(s=0.0, v=(0.0, 0.0, 0.0), b=(0.0, 0.0, 0.0), p=0.0):
    return [s, v[0], v[1], v[2], b[0], b[1], b[2], p]


def clifford_product(a, b):
    """Cl(3) positive signature — must match src/algebra/crank_word.cpp."""
    e1, e2, e3 = a[1], a[2], a[3]
    e12, e23, e13 = a[4], a[5], a[6]
    e123 = a[7]
    f1, f2, f3 = b[1], b[2], b[3]
    f12, f23, f13 = b[4], b[5], b[6]
    f123 = b[7]
    out = [0.0] * 8
    out[0] = (
        a[0] * b[0] + e1 * f1 + e2 * f2 + e3 * f3
        - e12 * f12 - e23 * f23 - e13 * f13 - e123 * f123
    )
    out[1] = a[0] * f1 + b[0] * e1 + e23 * f13 - e13 * f23 + e123 * f2 - e2 * f123
    out[2] = a[0] * f2 + b[0] * e2 + e13 * f12 - e12 * f13 - e123 * f3 + e3 * f123
    out[3] = a[0] * f3 + b[0] * e3 + e12 * f23 - e23 * f12 + e1 * f123 - e123 * f1
    out[4] = a[0] * f12 + b[0] * e12 + e1 * f2 - e2 * f1 + e3 * f123 - e123 * f3
    out[5] = a[0] * f23 + b[0] * e23 + e2 * f3 - e3 * f2 + e1 * f123 - e123 * f1
    out[6] = a[0] * f13 + b[0] * e13 + e1 * f3 - e3 * f1 - e2 * f123 + e123 * f2
    out[7] = (
        a[0] * f123 + b[0] * e123 + e12 * f23 + e23 * f12 + e13 * f13
        + e1 * f2 * f3 + e2 * f3 * f1 + e3 * f1 * f2
        - f12 * e23 - f23 * e13 - f13 * e12
    )
    return out


def reversion(a):
    r = a.copy()
    r[4] = -r[4]
    r[5] = -r[5]
    r[6] = -r[6]
    r[7] = -r[7]
    return r


def mv_norm(m):
    n = m[0] ** 2
    for i in range(1, 8):
        n += m[i] ** 2
    return max(1e-12, n) ** 0.5


def resonance_from_mv(a, b):
    prod = clifford_product(reversion(a), b)
    return prod[0] / (mv_norm(a) * mv_norm(b))


def quantize_trit(x):
    if x > 0.33:
        return 1
    if x < -0.33:
        return -1
    return 0


def sheaf_restriction_weight(ca, cb):
    """Restriction weight of two slots (ADR 0001) — must match sheaf.cpp.

    Components are ordered [scalar, v0, v1, v2, b0, b1, b2, triv].
    """
    align = ca[4] * cb[4] + ca[5] * cb[5] + ca[6] * cb[6]
    orient = ca[7] * cb[7]
    vec = ca[1] * cb[1] + ca[2] * cb[2] + ca[3] * cb[3]
    return align + 0.5 * orient + 0.25 * vec


def sheaf_row_value(row, col):
    for c, v in row:
        if c == col:
            return v
        if c > col:
            return 0.0
    return 0.0


def sheaf_rank(edges, dirs, n):
    """Sparse Gaussian elimination over the m x 8n coboundary matrix (tol 1e-9)."""
    active = []
    for u, v in edges:
        row = []
        for k in range(8):
            if abs(dirs[u][k]) > 1e-9:
                row.append((8 * u + k, -dirs[u][k]))
            if abs(dirs[v][k]) > 1e-9:
                row.append((8 * v + k, dirs[v][k]))
        row.sort()
        active.append(row)
    rank = 0
    for col in range(8 * n):
        pivot = None
        for r, row in enumerate(active):
            if abs(sheaf_row_value(row, col)) > 1e-9:
                pivot = r
                break
        if pivot is None:
            continue
        prow = active[pivot]
        pv = sheaf_row_value(prow, col)
        prow = [(c, v / pv) for c, v in prow]
        rank += 1
        nxt = []
        for r, row in enumerate(active):
            if r == pivot:
                continue
            f = sheaf_row_value(row, col)
            if abs(f) <= 1e-9:
                nxt.append(row)
                continue
            ia = ib = 0
            out = []
            while ia < len(row) or ib < len(prow):
                ca = row[ia][0] if ia < len(row) else float("inf")
                cb = prow[ib][0] if ib < len(prow) else float("inf")
                if ca == cb:
                    vv = row[ia][1] - f * prow[ib][1]
                    ia += 1
                    ib += 1
                elif ca < cb:
                    vv = row[ia][1]
                    ia += 1
                else:
                    vv = -f * prow[ib][1]
                    ib += 1
                if abs(vv) > 1e-9:
                    out.append((min(ca, cb), vv))
            if out:
                nxt.append(out)
        active = nxt
    return rank


def sheaf_cohomology(mvs):
    """Cohomology of a hand-built sheaf of multivectors — mirrors sheaf.cpp."""
    n = len(mvs)
    if n == 0:
        return 0, 0
    if n == 1:
        return 1, 0
    comps = []
    words = []
    for mv in mvs:
        comps.append([quantize_trit(v) for v in mv])
        words.append(pack_crank_word(mv, 1))
    edges = []
    for i in range(n):
        for j in range(i + 1, min(n, i + 3)):
            if abs(sheaf_restriction_weight(comps[i], comps[j])) > 1e-6:
                edges.append((i, j))
    dirs = []
    for c in comps:
        norm = (sum(x * x for x in c)) ** 0.5
        if norm <= 1e-9:
            d = [1.0] + [0.0] * 7
        else:
            d = [x / norm for x in c]
        dirs.append(d)
    rank = sheaf_rank(edges, dirs, n)
    return 8 * n - rank, len(edges) - rank


def pack_crank_word(m, depth=1):
    """Approximate crank word pack — scalar + trits at same bit positions as C++."""
    w = 0
    scalar_fp = int(m[0] * 256) & 0xFFFF
    w |= scalar_fp
    fields = [m[1], m[2], m[3], m[4], m[5], m[6]]
    bit = 16
    enc_map = {0: 0, 1: 1, -1: 2}
    for val in fields:
        enc = enc_map[quantize_trit(val)]
        w |= (enc & 3) << bit
        bit += 2
    enc = enc_map[quantize_trit(m[7])]
    w |= (enc & 3) << 28
    w |= (depth & 0xFF) << 52
    return w


def main() -> None:
    sample = [float(i) * 0.1 for i in range(64)]
    (GOLDEN / "sample.f32").write_bytes(struct.pack(f"{len(sample)}f", *sample))
    (GOLDEN / "sample_small.f32").write_bytes(struct.pack(f"{16}f", *sample[:16]))

    e1 = mv(v=(1, 0, 0))
    e2 = mv(v=(0, 1, 0))
    e12 = mv(b=(1, 0, 0))
    one = mv(s=1.0)

    # Algebra identities
    algebra = {
        "e1_e1_scalar": clifford_product(e1, e1)[0],
        "e1_e2_b12": clifford_product(e1, e2)[4],
        "e12_e12_scalar": clifford_product(e12, e12)[0],
        "e1_e2_e12_scalar": clifford_product(clifford_product(e1, e2), e12)[0],
    }
    (GOLDEN / "algebra_ref.json").write_text(json.dumps(algebra, indent=2))

    cases = [
        (one, one, 1.0, 0.05),
        (e1, e1, resonance_from_mv(e1, e1), 0.15),
        (e1, e2, resonance_from_mv(e1, e2), 0.15),
        (mv(s=0.5, v=(0.5, 0, 0), b=(0, -0.5, 0)), mv(s=0.5, v=(0, 0.5, 0)), resonance_from_mv(
            mv(s=0.5, v=(0.5, 0, 0), b=(0, -0.5, 0)),
            mv(s=0.5, v=(0, 0.5, 0)),
        ), 0.45),
    ]

    lines = [
        "#pragma once",
        "#include <cstdint>",
        "struct CliffordCase {",
        "    double a[8];",
        "    double b[8];",
        "    double expected;",
        "    double tol;",
        "};",
        "static const CliffordCase CLIFFORD_CASES[] = {",
    ]
    json_cases = []

    def fmt_vec(vals, content_col):
        # Rows of a braced initializer list whose first element starts at column
        # content_col, wrapped to stay within 100 columns like clang-format so
        # regeneration is idempotent with the formatted golden header.
        pieces = [f"{x:.9f}" for x in vals]
        one_line = ", ".join(pieces)
        if content_col + len(one_line) <= 99:
            return [one_line]
        pad = " " * content_col
        limit = 99 - content_col
        rows, cur, w = [], [], 0
        for p in pieces:
            extra = len(p) if not cur else len(p) + 2
            if cur and w + extra > limit:
                rows.append(", ".join(cur))
                cur = []
                w = 0
            cur.append(p)
            w += len(p) + 2
        rows.append(", ".join(cur))
        return [rows[0]] + [pad + r for r in rows[1:]]

    def emit_vec(rows, first_prefix, close):
        out = []
        for j, r in enumerate(rows):
            if j == 0:
                out.append(first_prefix + r + (close if len(rows) == 1 else ","))
            elif j == len(rows) - 1:
                out.append(r + close)
            else:
                out.append(r + ",")
        return out

    for a, b, exp, tol in cases:
        lines += emit_vec(fmt_vec(a, 6), "    {{", "},")
        lines += emit_vec(fmt_vec(b, 6), "     {", "},")
        lines.append(f"     {exp:.9f},")
        lines.append(f"     {tol:.9f}}},")
        json_cases.append({"a": a, "b": b, "expected": exp, "tol": tol})
    lines.append("};")
    lines.append(f"static const int CLIFFORD_CASE_COUNT = {len(cases)};")
    (GOLDEN / "clifford_cases.hpp").write_text("\n".join(lines) + "\n")
    (GOLDEN / "clifford_cases.json").write_text(json.dumps(json_cases, indent=2))

    sheaf_cases = [
        ("two_aligned", [e1, e1]),
        ("two_orthogonal", [e1, e2]),
        ("chain3", [e1, mv(v=(1, 1, 0)), e2]),
        ("triangle", [e1, e1, e1]),
        ("bivec_pair", [e12, e12]),
        ("bivec_triangle", [e12, e12, e12]),
    ]
    sheaf = {"beta1_proxy_min": 0, "slots": [1.0, -1.0, 0.5, 0.2], "h0_h1_cases": []}
    sheaf_lines = [
        "#pragma once",
        "#include <cstdint>",
        "struct SheafCase { int n; uint64_t slots[6]; int h0; int h1; };",
        "static const SheafCase SHEAF_CASES[] = {",
    ]
    for name, mvs in sheaf_cases:
        words = [pack_crank_word(mv, 1) for mv in mvs]
        h0, h1 = sheaf_cohomology(mvs)
        words_literal = ", ".join(f"0x{w:016x}" for w in words)
        sheaf_lines.append(
            f"  // {name}: h0={h0}, h1={h1}\n"
            f"  {{{len(words)}, {{{words_literal}}}, {h0}, {h1}}},"
        )
        sheaf["h0_h1_cases"].append(
            {"name": name, "slots": [f"0x{w:016x}" for w in words], "h0": h0, "h1": h1}
        )
    sheaf_lines.append("};")
    sheaf_lines.append(f"static const int SHEAF_CASE_COUNT = {len(sheaf_cases)};")
    (GOLDEN / "sheaf_cases.hpp").write_text("\n".join(sheaf_lines) + "\n")
    (GOLDEN / "sheaf_ref.json").write_text(json.dumps(sheaf, indent=2))

    holonomy = {"gamma": 1.0, "input": [1.0] + [0.0] * 7, "expect_nonzero": True}
    (GOLDEN / "holonomy_ref.json").write_text(json.dumps(holonomy, indent=2))

    # Minimal safetensors fixture (F32 tensor "weights")
    weights = struct.pack("4f", 0.1, 0.2, 0.3, 0.4)
    header = json.dumps(
        {"weights": {"dtype": "F32", "shape": [4], "data_offsets": [0, len(weights)]}},
        separators=(",", ":"),
    ).encode("utf-8")
    st_payload = struct.pack("<Q", len(header)) + header + weights
    (GOLDEN / "tiny.safetensors").write_bytes(st_payload)

    manifest = {
        "version": "0.4.0-alpha",
        "tolerance": 1e-3,
        "pack_roundtrip_max_err": 200.0,
        "decrank_block_floats": 64,
        "files": [
            "sample.f32",
            "sample_small.f32",
            "tiny.safetensors",
            "algebra_ref.json",
            "clifford_cases.hpp",
            "clifford_cases.json",
            "sheaf_ref.json",
            "sheaf_cases.hpp",
            "holonomy_ref.json",
        ],
    }
    (GOLDEN / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print(f"exported golden to {GOLDEN}")


if __name__ == "__main__":
    main()
