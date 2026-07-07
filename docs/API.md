# Crankle C API (v0.2.2)

Modular headers live under `include/crankle/`. Include `crankle/crankle.h` for everything, or pull in domain headers individually.

## Header layout

| Header | Domain |
|--------|--------|
| `types.h` | Shared structs (`crankle_cran_t`, multivector, metadata) |
| `errors.h` | `CRANKLE_OK`, `crankle_strerror()` |
| `version_api.h` | `crankle_version_string()`, `crankle_version()` |
| `trit.h` | Ternary encode/decode |
| `crank.h` | Crank word pack/unpack, decrank matrix |
| `clifford.h` | Geometric product, resonance |
| `sheaf.h` | Sheaf resonance, β₁ proxy |
| `symplectic.h` | Turn, peel, bind |
| `pack.h` | Float fold/unfold |
| `cran.h` | `.cran` archive I/O + metadata footer |
| `holonomy.h` | Wilson holonomy forward |
| `diff.h` | Crank tensor diff metrics |
| `simd.h` | Runtime AVX2 probe |

## Implementation layout

C bindings are split under `src/c_api/` — one translation unit per domain, with `internal.hpp` handling multivector marshalling.

## New in v0.2.2

| Function | Description |
|----------|-------------|
| `crankle_version_string` | Runtime version string |
| `crankle_version` | Major/minor/patch components |
| `crankle_strerror` | Human-readable status codes |
| `crankle_cran_write_with_metadata` | Write `.cran` v2 with JSON footer |
| `crankle_cran_read_metadata` | Read model name + source hash footer |

## Error codes

| Code | Meaning |
|------|---------|
| `CRANKLE_OK` (0) | Success |
| `CRANKLE_ERR_NULL` | Null pointer argument |
| `CRANKLE_ERR_INVALID` | Invalid argument |
| `CRANKLE_ERR_IO` | File I/O failure |
| `CRANKLE_ERR_FORMAT` | Archive format/checksum failure |
| `CRANKLE_ERR_NO_METADATA` (1) | Archive has no metadata footer |

## CLI

```bash
crankle version
crankle diff a.cran b.cran
```

## Core functions

| Function | Description |
|----------|-------------|
| `crankle_pack_f32` | Fold float buffer into crank slots (topological annealing) |
| `crankle_unpack_f32` | Unfold crank slots to floats |
| `crankle_clifford_resonance` | Clifford inner product resonance |
| `crankle_sheaf_resonance` | Sheaf χ proxy resonance |
| `crankle_turn` | Symplectic BCH crank turn step |
| `crankle_peel` | RG peel layers |
| `crankle_bind` | Clifford product bind |
| `crankle_cran_read/write` | `.cran` I/O |
| `crankle_holonomy` | Wilson holonomy forward |
| `crankle_crank_diff_count` | Slots that differ between tensors |
| `crankle_crank_diff_hamming` | Normalized bit Hamming distance |
| `crankle_has_avx2` | Runtime SIMD probe |
