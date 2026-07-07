# Crankl C API (v0.4.0)

Modular headers live under `include/crankl/`. Include `crankl/crankl.h` for everything, or pull in domain headers individually.

## Header layout

| Header | Domain |
|--------|--------|
| `types.h` | Shared structs (`crankl_cran_t`, multivector, metadata) |
| `errors.h` | `CRANKL_OK`, `crankl_strerror()` |
| `version_api.h` | `crankl_version_string()`, `crankl_version()` |
| `trit.h` | Ternary encode/decode |
| `crank.h` | Crank word pack/unpack, decrank matrix |
| `clifford.h` | Geometric product, resonance |
| `sheaf.h` | Sheaf resonance, β₁ proxy |
| `symplectic.h` | Turn, peel, bind, finetune, holonomy MSE |
| `pack.h` | Float fold/unfold, decrank loss, block tiling |
| `cran.h` | `.cran` archive I/O + metadata footer |
| `holonomy.h` | Wilson holonomy forward |
| `diff.h` | Crank tensor diff metrics |
| `simd.h` | Runtime AVX2 probe |
| `metrics.h` | Archive health metrics for AI dev-flow automation |

## Implementation layout

C bindings are split under `src/c_api/` — one translation unit per domain, with `internal.hpp` handling multivector marshalling.

## New in v0.2.2

| Function | Description |
|----------|-------------|
| `crankl_version_string` | Runtime version string |
| `crankl_version` | Major/minor/patch components |
| `crankl_strerror` | Human-readable status codes |
| `crankl_cran_write_with_metadata` | Write `.cran` v2 with JSON footer |
| `crankl_cran_read_metadata` | Read model name + source hash footer |

## Error codes

| Code | Meaning |
|------|---------|
| `CRANKL_OK` (0) | Success |
| `CRANKL_ERR_NULL` | Null pointer argument |
| `CRANKL_ERR_INVALID` | Invalid argument |
| `CRANKL_ERR_IO` | File I/O failure |
| `CRANKL_ERR_FORMAT` | Archive format/checksum failure |
| `CRANKL_ERR_NO_METADATA` (1) | Archive has no metadata footer |

## CLI

```bash
crankl version
crankl diff a.cran b.cran
crankl inspect a.cran --json
crankl compare baseline.cran tuned.cran --json
crankl pipeline --input weights.f32 -o tuned.cran --manifest run.json
crankl finetune --input adapter.cran --target weights.f32 --steps 200 -o tuned.cran --json
```

## New in v0.4.0

| Function | Description |
|----------|-------------|
| `crankl_pack_n_slots` | Slots required for N floats (64 floats per slot) |
| `crankl_unpack_f32_mode` | Unpack decrank blocks or legacy coeffs |
| `crankl_decrank_frobenius_loss` | Per-slot reconstruction loss vs 64-float block |
| `crankl_finetune` | Maurer-Cartan loop with recon + task loss callback |
| `crankl_holonomy_mse` | Calibration MSE via Wilson holonomy forward |
| `crankl_peel_stack` | Roll back N layers from cran v2 stack history |

## Core functions

| Function | Description |
|----------|-------------|
| `crankl_pack_f32` | Fold float buffer into crank slots (topological annealing) |
| `crankl_unpack_f32` | Unfold crank slots to floats |
| `crankl_clifford_resonance` | Clifford inner product resonance |
| `crankl_sheaf_resonance` | Sheaf χ proxy resonance |
| `crankl_turn` | Symplectic BCH crank turn step |
| `crankl_peel` | RG peel layers |
| `crankl_bind` | Clifford product bind |
| `crankl_cran_read/write` | `.cran` I/O |
| `crankl_holonomy` | Wilson holonomy forward |
| `crankl_crank_diff_count` | Slots that differ between tensors |
| `crankl_crank_diff_hamming` | Normalized bit Hamming distance |
| `crankl_has_avx2` | Runtime SIMD probe |
| `crankl_compute_archive_metrics` | Density, entropy, energy, depth, and β₁ metrics |
| `crankl_cran_compute_metrics` | Archive metrics directly from a mapped `.cran` |
