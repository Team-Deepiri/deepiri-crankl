# Crankle C API

See `include/crankle/crankle.h`.

## Core types

- `crankle_crank_t` — 64-bit crank word wrapper
- `crankle_multivector_t` — Cl(3) multivector components
- `crankle_cran_t` — mmap'd archive view

## Functions

| Function | Description |
|----------|-------------|
| `crankle_pack_f32` | Fold float buffer into crank slots |
| `crankle_unpack_f32` | Unfold crank slots to floats |
| `crankle_clifford_resonance` | Clifford inner product resonance |
| `crankle_sheaf_resonance` | Sheaf χ proxy resonance |
| `crankle_turn` | Symplectic crank turn step |
| `crankle_peel` | RG peel layers |
| `crankle_bind` | Clifford product bind |
| `crankle_cran_read/write` | `.cran` I/O |
| `crankle_holonomy` | Wilson holonomy forward |
