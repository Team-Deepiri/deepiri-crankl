# Changelog

All notable changes to crankl are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning is SemVer.

## [1.0.0] — 2026-08-25

First stable release: Windows support, CPack installable packages, and the
permanent "alpha" label removed.

### Added

- **Windows x86_64 support**: the `.crank` reader now uses the Win32 API
  (`CreateFileMappingW` / `MapViewOfFile`) on `_WIN32` builds; CI runs a full
  msys2/MinGW build + test on `windows-latest`. The library compiles out of
  the box on all three major desktop platforms; Windows builds share the same
  C API, linear cohomology, and test battery. (POSIX mmap reader unchanged.)
- **CPack packaging**: `cpack -G TGZ` / `ZIP` / `DEB` produces installable
  tarballs, a Windows ZIP, and a `.deb` in one command after build. Release
  artifacts include a Windows binary tarball alongside the existing Linux one.

### Changed

- `CRANKL_VERSION_STRING` is now `"1.0.0"` (dropping the `-alpha` suffix).
- Removed stale `<unistd.h>` includes from test translation units; temp paths
  now use `std::filesystem::temp_directory_path()` for cross-platform safety.

## [0.5.2-alpha] — 2026-08-24

Hardening pass: linear-time cohomology, thread-safety contract, production-scale
stress testing, coverage-guided fuzzing, macOS CI, and one-shot setup.

### Fixed

- **Cohomology is now actually O(n)**: the public `crankl_sheaf_cohomology`
  path computed the ADR 0001 rank theorem via explicit sparse Gaussian
  elimination, which measured quadratic in practice (32k slots ≈ 113 s;
  1M slots would take days) — contradicting the report's cost claim. The
  theorem gives `rank(delta_0) = n − c` in closed form, so the production path
  now counts connected components with a union-find instead: **32k slots in
  16 ms, 1M slots in 337 ms (~7000× at 32k)**, bitwise-identical results.
  The elimination path is retained as an internal reference and cross-checked
  against the fast path on 280 randomized archives per test run.
- **CLI accepts `--output` everywhere** `-o` was accepted; previously the flag
  was silently ignored and commands exited 1 with no message.
- Stress harness no longer pollutes captured JSON output.

### Added

- **Thread-safety contract, tested**: the reader keeps no shared mutable state
  (verified by symbol audit); new `test_threading` runs 8 threads × 20 open/
  forward/close cycles on independent handles of the same archive and requires
  bitwise equality with the serial reference. Contract documented in
  `include/crankl/types.h`.
- **Scale stress test** (`scripts/stress_test.sh`): 256 MiB pack → inspect →
  holonomy under time/RSS ceilings; measured ceilings documented in-script.
  Found both CLI bugs above on its first run.
- **Coverage-guided fuzzing**: three libFuzzer targets (`tests/fuzz/`) for the
  `.crank`, `.safetensors`, and GGUF parsers behind
  `CRANKL_BUILD_FUZZERS=ON` (clang). Local campaigns: ~1.9 M executions across
  the three parsers, zero crashes. Complements the deterministic mutation
  harness (`test_fuzz_parsers`).
- **macOS CI job**: full Release build + ctest on macos-latest (scalar SIMD
  fallback; AVX2 probe auto-disables on arm64).
- **One-shot setup** (`./setup.sh`): installs system deps (apt/dnf/pacman/
  brew), configures, builds, tests; `--with-gui`, `--with-fuzzers`,
  `--install [PREFIX]`, `--no-deps`.

## [0.5.1-alpha] — 2026-08-24

Productization pass: export hygiene, downstream packaging, parser fuzzing,
tunable cohomology gate, and research-report depth.

### Added

- **Tunable cohomology gate** (`crankl_sheaf_cohomology_tol`): caller-chosen
  edge-restriction threshold; the default path keeps 1e-6. Looser thresholds
  monotonically weaken the restriction graph (h1 falls toward the disconnected
  limit); non-finite / non-positive tolerances return `CRANKL_ERR_INVALID`.
- **Single-tensor read** (`crankl_safetensors_read_f32`): malloc'd f32 buffer +
  element count for one named tensor of a `.safetensors` file.
- **`find_package(crankl)` support**: installs `crankl::crankl` CMake target
  with config/version files under `lib/cmake/crankl`; verified by a consumer
  build against a fresh install tree.
- **Parser robustness harness** (`tests/ctest/test_fuzz_parsers`): 12 000
  mutated inputs per run across the .crank, safetensors, and GGUF readers;
  deterministic seeds, runs in CI and under ASan/UBSan.
- **Research report**: shape-scaling table (§4.4), drift-gate families (§4.5:
  benign noise invariance, row-decay collapse h1→0/h0→8n, mode-collapse
  inflation +4%), and a gated-trajectory study (§4.6: alarm at epoch 5/12 while
  Frobenius error moved 2%). All tables regenerate via `tools/bench_sweep.c`.
- Man page `docs/crankl.1`, `CONTRIBUTING.md`, `SECURITY.md`.
- CI: Qt6 GUI build job running the offscreen `--smoke-test`; README badges.

### Changed

- **Export surface locked**: the shared object now exports only `crankl_*`
  symbols (GNU/Clang ELF version script); ~100 previously leaked C++
  implementation symbols are hidden. The CLI now consumes only public API.
- Version reporting unified on project version (0.5.1-alpha).

## [0.5.0-alpha] — 2026-08-21

Production release: multi-tensor checkpoint ingest, sheaf cohomology quality
metrics, batched AVX2 holonomy inference, selectable pack objectives, and the
phase-2 GUI. Backward compatible — no existing public symbol changed, format v2
archives unchanged, legacy v1/v2 fixtures still pass.

### Added

- **Sheaf cohomology QC** (`crankl_sheaf_cohomology`, `crankl_sheaf_h0_dim`,
  `crankl_sheaf_h1_dim`, `crankl_sheaf_resonance_h1`): h0 counts independent
  global sections (7n + c), h1 counts obstruction cycles (rank theorem
  h1 = m − n + c). Exposed in `inspect` (text + JSON) and `compare`.
  Design record in `docs/adr/0001-sheaf-cohomology.md`.
- **Multi-tensor ingest**: `crankl_pack_safetensors_multi` packs every F32 tensor
  of a `.safetensors` checkpoint into ONE archive with a tensor index in the META
  footer (per-tensor xxh64 checksums, ≤32 tensors/archive) and manifest schema
  v2 (`parent_run_id`, `peels_applied`, `finetune_loss_curve`, `tensors[]`).
  Read back with `crankl_archive_tensor_count/list`. Format addendum in
  `docs/CRANK_FORMAT.md`.
- **GGUF ingest** (`crankl_gguf_count/list`, `crankl_pack_gguf_f32`): GGUF
  v1–v3 reader; F32 packed natively, F16 widened. Smoke-grade by design.
- **Batched holonomy** (`crankl_holonomy_batch`, `crankl_holonomy_mse_batch`,
  `crankl_holonomy_avx2_supported`): per-slot Padé exponentials hoisted out of
  the batch loop, applied through an AVX2/FMA mat-vec kernel. Bitwise identical
  to serial calls; up to 171× faster at LoRA scale (4096×64, batch 1024).
  CLI: `holonomy --batch N`.
- **Pack objective modes** (`crankl_pack_f32_anneal`, `crankl_pack_objective`,
  `crankl_pack_default_mode`; `CRANKL_PACK_MODE_{LEGACY,STAGED,BO}`): mode 0 is
  byte-identical to `crankl_pack_f32`, modes 1–2 refine slot words against a
  weighted objective and are deterministic per seed. CLI:
  `pack --pack-mode legacy|staged|bo`.
- **GUI phase 2**: real compare via the C API, QProcess-driven CLI jobs
  (pack/turn/finetune/peel), ComparePage, NewJobDialog, smoke-test flag,
  theme polish. CLI path configurable via `CRANKL_CLI`.
- Benchmarks: `tools/bench_sweep.c` (reproducible report tables),
  `tools/bench_holonomy.c`, `tools/bench_pack.c`;
  `scripts/bench_finetune.sh` and `scripts/bench_pack.sh` upgraded to
  LoRA-scale runs.

### Changed

- `inspect --json` output extended with `cohomology` object and `tensors[]`
  index for multi-tensor archives.
- Golden exporter (`scripts/export_golden.py`) emits clang-format-compatible
  wrapping so regeneration is idempotent with CI-formatted sources.

### Fixed

- **Peel safety**: the reader now sets `layers = NULL` when no validated stack
  section exists. Previously a fallback pointer just past the slots could alias
  the META footer and be peeled as history.
- **Batched holonomy aliasing**: the dense batched apply wrote to its own input,
  feeding overwritten lanes into later rows (up to 1.4e+02 error at LoRA scale).
  Results now land in a scratch buffer; outputs match serial exactly.
- Safetensors header parsing tolerates whitespace between JSON keys and values.
- ASan global-buffer-overflow in an ingest test fixture; strncpy truncation
  warnings silenced in `bind_cran.cpp`.

## [0.4.0-alpha]

- `.crank` format v2: metadata footer, layer history stacks, per-handle mmap
  ownership, checksummed payloads.
- Decrank pack/unpack pipeline, finetune loop, archive metrics, CLI flow
  commands (`pipeline`, `inspect`, `compare`), Qt GUI phase 1.
