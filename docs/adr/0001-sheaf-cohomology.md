# ADR 0001 — Restriction sheaf cohomology for crank archives

**Status:** accepted (track D1 + I2)
**Date:** 2026-08-19

## Context

`sheaf_beta1_proxy` reports cycle rank of the *adjacent-slot path* graph,
`|E| - |V| + components`. Because it only ever considers consecutive index
pairs, that graph is a disjoint union of paths, so the proxy is identically
zero for every archive — it is a degenerate diagnostic, not cohomology.
`sheaf_resonance` mixes it into an alternating sum. The plan (docs/NEXT_PHASE.md
D1/I2) requires a real coboundary operator δ and H⁰/H¹ dimensions.

## Decision

Build a **restriction sheaf on crank slots** whose cohomology replaces the
β₁ proxy.

### Stalks

Each crank word unpacks to a `Multivector` with 8 components: scalar, 3
vectors, 3 bivectors, pseudoscalar (see `src/internal_headers/algebra.hpp`).
The stalk over slot *i* is `F_i = R^8`, the multivector component space of
that slot. A section `f ∈ C⁰ = ⊕_i F_i = R^{8n}` assigns one multivector value
to every slot.

### Slot graph and restriction maps

The graph `G` has one vertex per slot. An undirected edge `(i, j)` exists iff

    1 ≤ j − i ≤ 2            (index neighbors, window 2)
    |restriction_map(slots[i], slots[j])| > ε,  ε = 1e-6

using the existing `restriction_map` helper unchanged (bivector alignment +
0.5·pseudoscalar orientation + 0.25·vector alignment) and the same threshold
the proxy already used. The window-2 edges are what make nontrivial cycles
(triangles) possible; distance-1 edges alone always form paths and would force
`dim H¹ = 0` for every archive, which is the failure mode of the proxy.

The edge stalk is `F_e = R`. Each vertex restricts to an edge by projection
along the slot's own principal multivector direction:

    ρ_{i ≤ e}(x) = ⟨û_i, x⟩,     û_i = unit multivector direction of slot i
                                   (fallback e₁ for the (near-)zero slot)

`û_i` is the normalized 8-component vector of the unpacked multivector. It is
documented as the "compatibility projection": an edge fiber compares the two
stalks along the directions the slots themselves define.

### Coboundary operator

δ₀ : C⁰ → C¹ = ⊕_e F_e = R^m, one coordinate per edge:

    (δ₀ f)_e = ⟨û_{j}, f_{j}⟩ − ⟨û_{i}, f_{i}⟩      for edge e = (i, j)

C¹ is the scalar obstruction space per edge; there are no 2-cells, so δ₁ = 0.

### Cohomology

    H⁰ = ker δ₀,            dim H⁰ = 8n − rank(δ₀)
    H¹ = C¹ / im δ₀,        dim H¹ = m − rank(δ₀)

**Rank theorem.** Each 8-column block of slot *i* is a scalar multiple of the
signed incidence column of vertex *i* in `G`, because column `8i+k` has entries
`±û_i[k]` on the edges incident to *i* only. Since `û_i ≠ 0` by normalization,
`rank(δ₀) = rank(signed incidence of G) = n − c`, where `c` is the number of
connected components of `G`. Hence the closed forms used for tests:

    dim H⁰ = 7n + c           (≥ 1 for any non-empty archive)
    dim H¹ = m − n + c        (≥ 0, cycle rank of the restriction slot graph)

**Constant global sections.** `f_i = c·û_i` lies in ker δ₀, so a connected
sheaf always carries the 1-dimensional constant-section subspace inside H⁰.

**Numerical contract.** `rank(δ₀)` is computed by Gaussian elimination over the
sparse `m × 8n` coefficient matrix, `O(n²)` worst case. A pivot is accepted only
when its absolute value exceeds `1e-9` relative to the matrix scale (entries are
`O(1)`, so this is ~7 orders above machine epsilon). Within a slot block the
columns are exact scalar multiples, so dependent rows cancel to ~1e-16 and are
correctly discarded; the closed-form `7n + c` / `m − n + c` is reproduced by the
elimination on all deterministic test and golden inputs.

### Trivial and boundary cases

| Archive | H⁰ | H¹ |
|---------|----|----|
| `n = 0` | 0  | 0  |
| `n = 1` | 1  | 0  |
| `n ≥ 2` | `7n + c` | `m − n + c` |

`n = 1` is pinned to `(1, 0)` by convention (trivial sheaf, one slot, no edges).

### Stability under peel, bind, and small trit surgery

`dim H¹ = m − n + c` depends on the edge set `{ (i,j) : |ρ(i,j)| > ε }` alone.
A perturbation of the slots changes `dim H¹` only when a pair's weight crosses
the `ε = 1e-6` threshold. Therefore

    |Δ dim H¹| ≤ number of window pairs whose |ρ(i,j)| changes by more
                 than (ε − |ρ(i,j)|) under the perturbation

which is 0 whenever every window pair is comfortably above the threshold.
`rg_peel` rescales bivector/pseudoscalar components (re-quantized to trits) and
leaves vector components untouched, so a vector-only archive is exactly
invariant. `bind_cranks` is a Clifford product and may move weights arbitrarily
far; the general bound above still applies pair-by-pair.

## Consequences

* `crankl_sheaf_beta1_proxy` and `crankl_sheaf_resonance` are kept as-is
  (deprecated aliases) so existing callers and the CLI keep working.
* New API: `crankl_sheaf_h0_dim`, `crankl_sheaf_h1_dim`,
  `crankl_sheaf_cohomology`, `crankl_sheaf_resonance_h1`.
* New code and JSON should report `h0_dim` / `h1_dim` instead of
  `beta1_proxy` (see cross-track notes in the implementing PR).
* The golden notebook (`02_sheaf_coboundary.ipynb`) and
  `scripts/export_golden.py` carry hand-built cases matching the C++ contract.