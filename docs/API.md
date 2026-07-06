# Crankle C API (v0.2)

See `include/crankle/crankle.h`.

## New in v0.2

| Function | Description |
|----------|-------------|
| `crankle_crank_diff_count` | Count slots that differ between crank tensors |
| `crankle_crank_diff_hamming` | Normalized bit Hamming distance |
| `crankle_has_avx2` | Runtime SIMD probe |

## CLI

```bash
crankle version
crankle diff a.cran b.cran
```

## Core types

- `crankle_crank_t` — 64-bit crank word wrapper
- `crankle_multivector_t` — Cl(3) multivector components
- `crankle_cran_t` — mmap'd archive view

## Functions

| Function | Description |
|----------|-------------|
| `crankle_pack_f32` | Fold float buffer into crank slots (annealing) |
| `crankle_unpack_f32` | Unfold crank slots to floats |
| `crankle_clifford_resonance` | Clifford inner product resonance |
| `crankle_sheaf_resonance` | Sheaf χ proxy resonance |
| `crankle_turn` | Symplectic crank turn step |
| `crankle_peel` | RG peel layers |
| `crankle_bind` | Clifford product bind |
| `crankle_cran_read/write` | `.cran` I/O |
| `crankle_holonomy` | Wilson holonomy forward |
