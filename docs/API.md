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
| `cran.h` | `.crank` archive I/O + metadata footer |
| `holonomy.h` | Wilson holonomy forward |
| `diff.h` | Crank tensor diff metrics |
| `simd.h` | Runtime AVX2 probe |
| `metrics.h` | Archive health metrics for AI dev-flow automation |

## Implementation layout

Sources are grouped by utility under `src/`. **Public** C headers live in `include/crankl/`
(installed). **Private** C++ headers live in `src/internal_headers/` (build-only, never installed).

### Source directories

| Directory | Responsibility |
|-----------|----------------|
| `src/algebra/` | Trits, crank words, Clifford product, resonance, 8×8 linalg / decrank |
| `src/topology/` | Sheaf resonance / β₁, RG peel / bind |
| `src/dynamics/` | Symplectic turn / finetune |
| `src/holonomy/` | Wilson holonomy forward |
| `src/pack/` | Float fold/unfold, persistence, block tiling |
| `src/archive/` | `.crank` reader/writer/metadata implementations |
| `src/ingest/` | Safetensors ingest |
| `src/metrics/` | Archive health metrics and crank diff |
| `src/simd/` | AVX2 dispatch |
| `src/c_bindings/` | Thin C ABI bindings (`bind_*.cpp`) — one file per public header domain |
| `src/cli/` | `crankl` CLI |

### Private headers (`src/internal_headers/`)

| Header | Contents |
|--------|----------|
| `algebra.hpp` | `Multivector`, trit encode, Clifford, decrank, 8×8 mats |
| `topology.hpp` | Sheaf resonance / β₁, RG peel / bind |
| `dynamics.hpp` | Symplectic turn / finetune |
| `metrics.hpp` | Archive metrics, crank diff |
| `holonomy.hpp` | Wilson forward |
| `pack.hpp` | Fold/unfold, tiling, persistence |
| `archive.hpp` | On-disk format, metadata, security limits, I/O decls |
| `ingest.hpp` | Safetensors |
| `simd.hpp` | AVX2 probe / kernels |
| `c_bindings.hpp` | C↔C++ multivector marshalling for `bind_*.cpp` |
| `api.hpp` | Convenience umbrella (archive + pack + holonomy + ingest) |

## New in v0.2.2

| Function | Description |
|----------|-------------|
| `crankl_version_string` | Runtime version string |
| `crankl_version` | Major/minor/patch components |
| `crankl_strerror` | Human-readable status codes |
| `crankl_cran_write_with_metadata` | Write `.crank` v2 with JSON footer |
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
crankl diff a.crank b.crank
crankl inspect a.crank --json
crankl compare baseline.crank tuned.crank --json
crankl pipeline --input weights.f32 -o tuned.crank --manifest run.json
crankl finetune --input adapter.crank --target weights.f32 --steps 200 -o tuned.crank --json
```

## New in v0.5.1

| Function | Description |
|----------|-------------|
| `crankl_sheaf_cohomology_tol` | Cohomology with a caller-chosen edge-restriction threshold (default path: 1e-6) |
| `crankl_safetensors_read_f32` | Read one tensor of a `.safetensors` file as malloc'd f32 |

### Packaging

- Shared object now exports **only** the `crankl_*` C API (linker version
  script); implementation internals are hidden. Code that reached into
  internals must move to the public API.
- `find_package(crankl)` support: installs `crankl::crankl` CMake target,
  config + version files under `lib/cmake/crankl`.
- `tests/ctest/test_fuzz_parsers` mutates 12 000 inputs across the three
  untrusted-input parsers (.crank, safetensors, GGUF) per run; runs in CI and
  under sanitizers.

## New in v0.5.0

| Function | Description |
|----------|-------------|
| `crankl_sheaf_cohomology` | h0/h1 dimensions of the slot sheaf (rank theorem: h1 = m − n + c) |
| `crankl_sheaf_h0_dim` / `crankl_sheaf_h1_dim` | Individual Betti numbers |
| `crankl_sheaf_resonance_h1` | h1-weighted resonance between two slot sets |
| `crankl_pack_f32_anneal` | Pack with mode: legacy (0), staged (1), BO-lite (2) |
| `crankl_pack_objective` | Weighted pack objective (w2 + λ·Frobenius) |
| `crankl_pack_default_mode` | Default pack mode for new artifacts |
| `crankl_safetensors_count/list` | Enumerate tensors of a `.safetensors` checkpoint |
| `crankl_gguf_count/list` | Enumerate tensors of a GGUF checkpoint (F32/F16 packable) |
| `crankl_pack_safetensors_multi` | All F32 tensors → ONE archive with tensor index footer |
| `crankl_pack_safetensors_tensor` | Single tensor → classic archive |
| `crankl_pack_gguf_f32` | GGUF smoke ingest (F16 widened to F32) |
| `crankl_archive_tensor_count/list` | Read back the multi-tensor index |
| `crankl_holonomy_batch` | Batched Wilson forward — per-slot exp built once, AVX2 applies |
| `crankl_holonomy_mse_batch` | Batch-mean calibration MSE |
| `crankl_holonomy_avx2_supported` | 1 when batched kernels use AVX2/FMA |

### CLI additions

- `pack --multi` packs every F32 tensor of a safetensors file into one archive.
- `pack --input model.gguf --tensor NAME` ingests GGUF checkpoints.
- `pack --pack-mode legacy|staged|bo` selects the pack objective mode.
- `inspect` reports cohomology h0/h1 and, for multi-tensor archives, the tensor index.
- `compare` reports `sheaf_resonance_h1`.
- `holonomy --batch N` runs batched forward over N stacked vectors.

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
| `crankl_cran_read/write` | `.crank` I/O |
| `crankl_holonomy` | Wilson holonomy forward |
| `crankl_crank_diff_count` | Slots that differ between tensors |
| `crankl_crank_diff_hamming` | Normalized bit Hamming distance |
| `crankl_has_avx2` | Runtime SIMD probe |
| `crankl_compute_archive_metrics` | Density, entropy, energy, depth, and β₁ metrics |
| `crankl_cran_compute_metrics` | Archive metrics directly from a mapped `.crank` |
