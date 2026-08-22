# Pack v3 — staged / topological-Bayesian anneal (design note)

**Pillar:** Roadmap D2 + I3 (docs/NEXT_PHASE.md).  
**Status:** implemented behind a flag; default path unchanged.

## Problem

`crankl_pack_f32` anneals each 8×8 tile greedily (continuous ±0.05 nudges of the
seed multivector, Boltzmann acceptance, then quantization to trits). The GUCT
target is J(C) = λ·W₂(PD) + μ·‖W − decrank(C)‖², but the v0.4 anneal never
re-visits trit coordinates after quantization and has no way to escape the
quantization-induced local optimum.

## 1. Search space

After the coarse fold each tile is a 64-bit crank word:

| Field | Bits | Free after coarse fold? |
|-------|------|------------------------|
| scalar (8.8 fixed point) | 0–15 | fixed by coarse fold |
| vec e₁,e₂,e₃ trits | 16–21 | **free** (ternary each) |
| bivec e₁₂,e₂₃,e₁₃ trits | 22–27 | **free** (ternary each) |
| trivector e₁₂₃ trit | 28–29 | **free** (ternary) |
| depth | 52–59 | fixed = 1 |
| flags | 60–63 | fixed = 0 |

The free refinement coordinates are the 7 ternary trit blades → 3⁷ = 2187
discrete states per tile. The proposal neighborhood is the 2-blade-flip
neighborhood: for each of the 7 blades, the two trit states different from the
current one (14 candidate words per tile step). This is strictly richer than
the continuous anneal's ±0.05 nudges because it can jump across the 0.33
quantization threshold.

## 2. Surrogate + acquisition (mode 2, BO-lite)

A full GP over 3⁷ per tile × many tiles is overkill for 64-float tiles whose
objective is cheap (one persistence diagram + one Frobenius). Approximation
chosen: a per-tile **sample-mean surrogate** with a UCB-style lower-confidence
bound.

- Surrogate posterior mean μ_s: running mean of the joint objectives observed
  on tile s (current word + all candidate words from each trial).
- Acquisition: pick the tile minimizing `μ_s − c·√(log(T+1)/(n_s+1))` where
  `n_s` is the number of trials on tile s and `c` is an exploration constant
  derived from the seed (deterministic). Minimization ⇒ "lower confidence
  bound"; the explore term keeps rarely-tried tiles from being starved.
- Candidate choice inside a tile: pick the candidate with the lowest joint
  objective; accept only on strict improvement.
- Determinism: no draws from the RNG; seed only modulates the exploration
  constant and tie-breaking. Same seed ⇒ bit-identical result.

**Why not a real GP:** tiles are independent (one word per tile), the objective
is cheap to evaluate exactly, and the acquisition is a 1-d scan over tiles. A
covariance fit buys nothing over the running-mean surrogate for this scale.

## 3. Objective + acceptance (modes 1 and 2)

Per-tile joint objective `α·W₂(PD_src, PD_decrank) + β·‖W−decrank(C)‖²` using
the existing 1D persistence diagrams (`persistence.cpp`). Both modes start from
the mode-0 fold result and **only apply strictly improving edits**, so the
archive-level objective
`obj(λ) = λ·Σ_s W₂_s + Σ_s frob_s` (as reported by `crankl_pack_objective`) is
never worse than mode 0 when `α = β = λ`.

## 4. Budget model

- Cap: `min(32 · n_slots, 2000)` tile-proposals per call (each proposal
  evaluates ≤14 candidate words). For a 16-slot archive this is ≤512 proposals;
  for a 64-slot archive 2000. Each candidate is one 8×8 decrank + one 1D PD
  (sort of 64 floats) ⇒ worst case well under a second even for 64 tiles.
- The default (mode 0) path does not consult the budget at all — byte-identical
  to v0.4.

## 5. Failure modes / fallback

| Failure mode | Behavior |
|--------------|----------|
| No improving edit found | Modes 1/2 return the mode-0 fold result unchanged (never worse) |
| Huge n_slots | Budget saturates at 2000 proposals; per-tile coverage shrinks, quality degrades toward mode 0 |
| Pathological data (NaNs/±Inf) | `crankl_pack_objective` returns finite or the packing returns the mode-0 slots; no crash |
| Surrogate variance collapse (mode 2) | Explore term keeps growing visits to all tiles; accepted edits still strictly improve |

Default recommendation: `--pack-mode 0` for fast packing, `1` for staged
refinement, `2` when exploration across tiles is wanted. Modes 1/2 are meant for
offline LoRA packing where a few extra milliseconds per tile is affordable.

## 6. Acceptance thresholds for LoRA A/B matrices

- A/B adapter blocks are 64 floats per 8×8 tile; packing must keep
  `Σ_s frob_s ≤ 0.05 · ‖W‖²` (≤5% relative reconstruction energy).
- Modes 1/2 additionally require `Σ_s W₂_s ≤ 0.01 · Σ_s W₂_s(mode0)` — the
  topological footprint must not regress.
- CI gate: on `tests/golden/sample.f32` and a rank-8 64×64 synthetic, mode 1
  and mode 2 must achieve `obj(1.0) ≤ obj(1.0)` of mode 0 within 1e-6.

## 7. Public API (v0.5 additions, all additive)

```c
int crankl_pack_f32_anneal(const float *data, size_t count, uint64_t *slots,
                           size_t n_slots, float alpha, float beta, int mode, unsigned seed);
int crankl_pack_objective(const float *data, size_t count, const uint64_t *slots,
                          size_t n_slots, double lambda, double *w2_out,
                          double *frobenius_out, double *objective_out);
int crankl_pack_default_mode(void);
```

`mode 0` is a thin wrapper over `crankl_pack_f32` (α→λ, β→μ) so existing call
sites are byte-identical. `crankl_pack_objective` returns totals and
`obj(λ) = λ·w2 + frobenius`; it is the function CI/tests use to assert
improvement.