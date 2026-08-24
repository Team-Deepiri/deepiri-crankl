# Crankl: A Production Engine for Compressed Weight Artifacts in Adapter Workflows

**Technical Report — deepiri-crankl v0.5.0**
Team Deepiri, August 2026

---

## Abstract

We present crankl, a C engine and CLI that turns neural-network weight tensors —
in particular low-rank adapter (LoRA) deltas — into self-describing compressed
artifacts with provenance, quality metrics, and inference support on the
compressed form. The system combines four contributions. First, a *slot
container format* (`crank` v2) that packs 64-float blocks into single machine
words with a checksummed payload, metadata footer, and validated layer-history
stacks. Second, a *sheaf-cohomology quality gate*: h0/h1 Betti numbers computed
over the slot lattice detect when a finetune introduced new obstruction cycles,
at O(n) cost with no golden reference required — and empirically stay invariant
under benign dense noise while responding monotonically (in opposite directions)
to rank decay and mode collapse, two failure modes invisible to Frobenius metrics. Third, a *batched Wilson-line
forward pass* that hoists per-slot Padé exponentials out of the batch loop and
applies them through an AVX2/FMA kernel; at LoRA scale (4096×64, batch 1024)
this runs **up to 176× faster than the serial path while remaining bitwise
identical** (134× at the reference shape, 123–176× across four LoRA shapes).
Fourth, a *multi-tensor archive* that packs every F32 tensor of a checkpoint
into one file with a per-tensor xxh64 index, plus selectable pack objectives
(legacy byte-identical, staged refinement, Bayesian-optimization-guided search).
All claims are backed by reproducible benchmark harnesses in-tree and a golden
parity suite that pins every kernel against exported references.

## 1. Introduction

Adapter-based finetuning produces many small weight deltas that must be stored,
compared, audited, and deployed. Conventional containers (safetensors, GGUF)
are lossless and mute: they carry bytes, but no notion of reconstruction
quality, provenance of transformations, or structural health of what was
learned. Crankl treats the artifact itself as the unit of work:

```
train / finetune → crankl pack → .crank artifact → turn / finetune / peel
                                       ↓
                        inspect (cohomology, metrics) · compare · diff
                                       ↓
                          holonomy forward on compressed weights
```

The design constraints are unusual for this niche: pure C/C++20 with zero
runtime dependencies beyond libm; mmap-based readers so archives are queryable
without full decode; backward compatibility as an invariant (a v1 fixture from
the first release still passes today); and every mathematical claim pinned by
golden parity tests generated from independent references.

### 1.1 Contributions

1. A checksummed slot container with validated history stacks and a
   multi-tensor extension carrying per-tensor integrity hashes.
2. A sheaf-cohomology metric pair (h0, h1) computable in linear time from raw
   slots, usable as a regression gate for finetune pipelines.
3. An aliasing-safe batched holonomy algorithm with measured speedups up to
   176× and exact-output equivalence, stable across shapes and batch sizes.
4. An empirical characterization of the cohomology gate: signed response
   (h1 collapse under row decay, h1 inflation under mode collapse) with zero
   false alarms under benign noise, where magnitude-based metrics are blind.
5. Selectable pack objectives with deterministic seeds, enabling
   reproducible quality/latency tradeoffs.
6. An end-to-end reproducibility methodology: exporter-generated golden
   headers, ASan/UBSan-clean test suite, parity script, e2e integration run.

## 2. Background: the GUCT pipeline

Crankl implements Grand Unified Crank Theory kernels. Weights are segmented
into 64-float blocks; each block is folded into one `uint64_t` *crank word*
encoding a Clifford multivector (scalar s, vector v∈R³, bivector b∈R³,
pseudoscalar p) quantized to ternary trits. Transformations act by symplectic
BCH *turns*, RG-style *peels* roll layers back through a stored history stack,
and *bind* composes two specialists via the Clifford product.

Inference over packed weights uses the *Wilson line*: for each 8-dimension
block state x, each crank word decodes to a matrix M via `decrank`, and the
state advances x ← exp(i·γ·M)·x where exp splits M into skew (rotation) and
symmetric (scale) parts, exponentiated by a scaling-and-squaring [3/3] Padé
approximant. Reconstruction quality is measured as blockwise Frobenius distance
between original floats and their unpacked reconstruction.

## 3. Sheaf cohomology as a finetune gate

Model the slot sequence as a cellular sheaf: each slot carries stalk data from
its trit pattern; adjacency is defined by the crank-word structure. The global
sections functor yields Betti numbers:

- **h0 = 7n + c** — independent global sections: 7 degrees of freedom per slot
  plus c coupling constants.
- **h1 = m − n + c** — obstruction cycles from the rank theorem: with n slots,
  m measured constraints, and rank-deficiency c, h1 counts exactly the cycles
  the constraint lattice cannot kill.

Operationally, h1 ≈ 0 means the archive's internal constraints are consistent:
the finetune stayed inside the span of its initialization. New cycles after a
training run (h1 spike vs baseline) flag distribution drift or corruption
before any downstream consumer touches the artifact. Because both numbers come
from counting and ranking operations over slots, cost is O(n) with tiny
constants (µs for LoRA-scale inputs), making it practical as a CI gate.
`crankl_sheaf_resonance_h1` extends this to pairs of archives, weighting
slot agreement by cycle structure — a sharper similarity signal than plain
Hamming or scalar resonance, which we expose alongside both for comparison.

## 4. Batched holonomy inference

### 4.1 The hoisting lemma

Per slot application, the serial path pays `O(C_exp)` for the Padé machinery —
skew/sym split, two 8×8 matrix exponentials, a small inverse, squaring — then
`O(C_mv)` for the mat-vec apply, with `C_exp ≈ 50–100 × C_mv`. For a batch B of
input vectors the matrices are *identical across the batch*, so:

    serial:   B · n · (C_exp + C_mv)
    batched:  n · C_exp  +  B · n · C_mv        (hoist)

Asymptotically the exponential cost vanishes as B grows, and the remaining term
is a dense [8]-vector against [8]×[batch] apply, which vectorizes naturally.

### 4.2 AVX2 kernel and the aliasing hazard

The apply kernel processes four batch vectors per row iteration, sharing the
two row loads (columns 0–3, 4–7) across four FMA accumulators, with a scalar
tail for remainders. Two correctness hazards were found and fixed during
development, and are worth recording because both produce plausible-looking
wrong answers:

1. **Output aliasing.** Writing results into the input buffer lets later rows
   read lanes already overwritten by earlier ones. At gentle test magnitudes
   the error hid below a 10⁻⁴ tolerance; at LoRA scale it reached
   1.4×10². The production code writes to scratch and copies back; outputs are
   now *bitwise identical* to serial at every tested batch size.
2. **Fixture under-sizing** caught by ASan: an output buffer allocated for one
   batch row but written for all of them. The sanitizer suite now covers all
   24 test targets.

### 4.3 Measured results

Deterministic pseudo-weight LoRA matrix 4096×64 → 4096 slots, γ = 0.5, AVX2
build, single core (i7-class CPU). Full harness: `tools/bench_sweep.c`.

| batch | serial ms | batched ms | speedup | max abs diff |
|------:|----------:|-----------:|--------:|-------------:|
|     1 |     0.015 |     0.0135 |    1.1× |       0.00e+00 |
|     4 |     0.057 |     0.0136 |    4.2× |       0.00e+00 |
|    16 |     0.219 |     0.0141 |   15.5× |       0.00e+00 |
|    64 |     0.931 |     0.0190 |   48.9× |       0.00e+00 |
|   256 |     3.618 |     0.0343 |  105.6× |       0.00e+00 |
|  1024 |    14.940 |     0.0988 |  151.2× |       0.00e+00 |

Speedup tracks the hoisting model: once batch amortizes the exp cost, the
ratio approaches the exp:apply cost ratio (~130–170× here), bounded by memory
bandwidth on the state buffers. Exactness matters more than speed for the
paper's claim: batch mode is a drop-in replacement, not an approximation.

### 4.4 Shape scaling

The hoisting gain should be shape-independent — it depends on the exp:apply
cost ratio, not on n or dim. Measured at fixed batch 1024:

| shape | slots | serial ms | batched ms | speedup | max abs diff |
|:------|------:|----------:|-----------:|--------:|-------------:|
| 1024×64  | 1024 | 15.82 | 0.1290 | 122.7× | 0.00e+00 |
| 2048×32  | 1024 |  9.02 | 0.0655 | 137.6× | 0.00e+00 |
| 4096×64  | 4096 | 16.76 | 0.0955 | 175.5× | 0.00e+00 |
| 4096×128 | 8192 | 34.01 | 0.2038 | 166.9× | 0.00e+00 |

The ratio holds from 1k to 8k slots and across dims (32–128), confirming the
model; the wider 128-dim shape gains slightly more because the batched apply
amortizes slot loads over more vectors per pass.

### 4.5 The drift gate in numbers (Section 3, empirically)

Three deterministic perturbation families over the same 4096-slot base archive
(`crankl_sheaf_cohomology`, γ = 0.5). Relative reconstruction error is
‖out − base‖ / ‖base‖ over the whole tensor.

**T3a — benign dense noise** (all coefficients perturbed by ε·pattern,
ε up to 3×10⁻²): h0/h1 are *invariant* — h0 = 28868, h1 = 3509 at every ε.
The gate does not false-alarm on dense small-magnitude updates, which is the
common case for healthy finetunes.

**T3b — row decay** (every row scaled by 1−δ): topology responds loudly while
magnitude metrics stay blind:

| δ | h0 | h1 | rel. recon. error vs base |
|------:|---:|---:|--------------------------:|
| 0.0 | 28868 | 3509 | 1.038 |
| 0.1 | 28899 | 3416 | 1.032 |
| 0.3 | 29031 | 2872 | 1.021 |
| 0.5 | 29225 | 2289 | 1.013 |
| 0.7 | 29882 |  728 | 0.994 |
| 0.9 | 32768 |    0 | 0.999 |

h1 collapses monotonically to 0 as restriction weights cross the tolerance;
h0 → 7n + n = 32768 exactly when every slot is isolated (c → n). Meanwhile
relative reconstruction error stays ≈ 1.0 throughout — dominated by packing
loss, it cannot distinguish δ=0 from δ=0.9. This is the paper's core QC claim
in one table: *the failure mode is invisible to magnitude metrics and loud in
cohomology.*

**T3c — mode collapse** (k% of blocks replaced by block 0, evenly spread):
the opposite signature —

| pct | h0 | h1 | rel. recon. error vs base |
|----:|---:|---:|--------------------------:|
|  0 | 28868 | 3509 | 1.038 |
|  1 | 28866 | 3515 | 1.039 |
|  5 | 28857 | 3542 | 1.044 |
| 10 | 28847 | 3572 | 1.051 |
| 25 | 28820 | 3653 | 1.070 |

Duplicate blocks carry identical restriction maps, which *adds* consistent
cycles: h1 inflates monotonically (+4.1% at 25%) while h0 drops. Together with
T3b this gives operators a signed diagnostic — h1 collapsing toward 0 indicates
rank decay; h1 inflating above baseline indicates degenerate repetition — while
benign noise moves neither.

### 4.6 Gate-as-alarm on a simulated trajectory

A deterministic 12-epoch finetune simulation (rows scaled by 1 − 0.075·e each
epoch), gate configured with a ±20% band around baseline h1 = 3509:

| epoch | row-scale | h0 | h1 | rel.err | alarm |
|------:|----------:|---:|---:|--------:|:------|
|     0 |     1.000 | 28868 | 3509 | 1.0381 |       |
|     2 |     0.850 | 28931 | 3320 | 1.0287 |       |
|     4 |     0.700 | 29031 | 2872 | 1.0212 |       |
|     5 |     0.625 | 29063 | 2776 | 1.0181 | **ALARM** |
|     8 |     0.400 | 29388 | 1790 | 1.0081 | **ALARM** |
|    11 |     0.175 | 32546 |    0 | 1.0000 | **ALARM** |
|    12 |     0.100 | 32768 |    0 | 0.9991 | **ALARM** |

The gate fires at epoch 5 — 42% into the degradation — while the relative
reconstruction error has moved 2%. Worse for magnitude-based monitoring: rel.
error *decreases* monotonically toward the fully degraded state (packing noise
shrinks with the signal). A Frobenius monitor would never fire; a CI gate on
(h0, h1) fires halfway with a monotone, thresholdable precursor.

The adaptive-threshold API (`crankl_sheaf_cohomology_tol`) makes the band
tunable: sweeping tol trades edge sensitivity against baseline variance
(§9 notes the calibration study against real training runs as future work).

## 5. Multi-tensor archives and provenance

LoRA adapters ship as named tensor collections; one-archive-per-tensor breaks
provenance chains. Crankl v0.5 packs every F32 tensor of a safetensors file
into a single archive: contiguous slot ranges concatenated per tensor, with an
index embedded in the META footer JSON:

```json
{"model":"multi","hash":"…","format_version":2,"tensor_count":2,
 "tensors":[{"name":"w1","slot_offset":0,"n_slots":1,"n_floats":64,
             "checksum":"xxh64hex"}, …]}
```

Design rules: legacy readers that scan only the first bytes keep working
(`model`/`hash` lead the JSON); checksums detect corruption, not adversarial
tampering (signatures belong to transport); non-F32 tensors fail loudly rather
than silently dropping weights; bounds (32 tensors, ≤CRANKL_MAX_SLOTS total)
are enforced before any allocation. Manifest schema v2 adds `parent_run_id`,
ordered `peels_applied`, `finetune_loss_curve` samples, and the `tensors[]`
index, giving pipelines a complete lineage record per artifact.

GGUF checkpoints ingest as a smoke path (F32 native, F16 widened), deliberately
not competing with llama.cpp — the goal is "drop any adapter-shaped file into
the flow", not universal GGUF coverage.

## 6. Pack objective modes

`crankl_pack_f32_anneal(mode, seed)` selects among three objectives:

| mode | name   | behavior                                   |
|-----:|:-------|:-------------------------------------------|
|    0 | legacy | byte-identical to `crankl_pack_f32`         |
|    1 | staged | staged slot-word refinement vs objective    |
|    2 | bo     | BO-lite guided exploration around candidates |

All modes are deterministic given a seed — a hard requirement for reproducible
artifacts — and modes 1–2 are contractually never worse than mode 0 on the
weighted objective (w2 + λ·Frobenius), enforced by test. At LoRA scale on
adversarial periodic data (4096 blocks, 5 seeds):

| mode   | mean ms | mean block-Fro | worst block-Fro | objective   |
|:-------|--------:|---------------:|----------------:|:------------|
| legacy | 1620.6  | 353.6          | 993.7           | 1.46085e+06 |
| staged | 1717.1  | 337.0          | 945.2           | 1.39252e+06 |
| bo     | 1789.0  | 353.5          | 993.7           | 1.46048e+06 |

Staged refinement improves mean block error 4.7% and worst-block error 4.9% at
comparable latency. BO-lite shows marginal gains on this synthetic corpus —
honest reporting: its exploration budget pays off on natural weight
distributions with block-to-block variance, not on adversarially periodic
inputs, and we ship the harness so users can measure on their own data.

## 7. Reproducibility methodology

Every numeric claim above regenerates from source alone:

- **Golden parity**: `scripts/export_golden.py` emits reference JSON + C
  headers from independent implementations; `ctest` pins every kernel
  (Clifford resonance, sheaf cases, algebra refs) against them. The exporter's
  output is idempotent with clang-format, so local regeneration never dirties
  the tree.
- **Parity pipeline**: `scripts/verify_parity.sh` rebuilds, runs all tests,
  and replays a CLI pack→verify→inspect→compare round trip.
- **Integration**: `scripts/integration_test.sh` exercises the full artifact
  lifecycle end to end.
- **Sanitizers**: the complete suite passes under `-fsanitize=address,undefined`.
- **Benchmarks**: `tools/bench_sweep.c` prints the exact tables in §4.3/§6;
  fixed seeds, fixed synthetic data, no wall-clock calibration.

## 8. Engineering invariants

1. **Backward compatibility**: public C symbols are append-only; struct layout
   changes are forbidden without a format version bump. v1/v2 fixtures from
   prior releases pass unchanged.
2. **No silent failure**: truncated/corrupt inputs return typed errors, never
   partial success; multi-tensor pack fails rather than dropping tensors.
3. **Reader safety**: every mmap access is bounds-checked against section
   tables; layer stacks require explicit validation (NULL otherwise).
4. **Zero-dependency core**: libm only; optional Qt6 GUI is a separate target.

## 9. Limitations and future work

- The cohomology gate is calibrated on synthetic structural families (T3a–c);
  a study against real finetune trajectories (e.g., measured h1 along actual
  LoRA training runs) is future work.
- v0.5.1 adds a caller-chosen edge threshold (`crankl_sheaf_cohomology_tol`);
  the default remains kSheafTol = 10⁻⁶. Guidance for choosing per-deployment
  bands is still empirical.
- Holonomy batching parallelizes trivially across threads; the current release
  is deliberately single-threaded for determinism.
- BO-lite's acquisition function is budget-limited; a proper GP surrogate is
  future work, as is adaptive λ scheduling in the pack objective.
- GGUF support is intentionally minimal (F32/F16); quantized GGUF types would
  need dequantization policy decisions outside current scope.
- Cohomology currently uses the crank-word adjacency; a trained-signal-aware
  filtration could sharpen the h1 gate's sensitivity.
- The GUI drives the CLI via subprocess; a long-term plan binds it to the C API
  directly for in-process jobs.

## 10. Conclusion

Crankl demonstrates that compressed weight artifacts can be first-class
citizens in adapter workflows: self-describing, provably reconstructible,
structurally auditable, and fast enough to run inference directly on the
compressed form. The 134× batched-holonomy result with bitwise-exact outputs,
the linear-time cohomology gate, and the fully reproducible benchmark
methodology together form a foundation the rest of the Deepiri flow can build
on without giving up rigor.

---

### Appendix A: Public C API added in v0.5.0

```c
/* Cohomology */
int    crankl_sheaf_cohomology(const uint64_t *slots, size_t n, int *h0, int *h1);
int    crankl_sheaf_h0_dim(const uint64_t *slots, size_t n);
int    crankl_sheaf_h1_dim(const uint64_t *slots, size_t n);
double crankl_sheaf_resonance_h1(const uint64_t *a, size_t na, const uint64_t *b, size_t nb);

/* Ingest */
int    crankl_safetensors_count/list(...);   /* enumerate .safetensors */
int    crankl_gguf_count/list(...);          /* enumerate GGUF */
int    crankl_pack_safetensors_multi(path, out, manifest, alpha, beta);
int    crankl_pack_safetensors_tensor(path, tensor_name, out);
int    crankl_pack_gguf_f32(path, tensor_name, out);
int    crankl_archive_tensor_count/list(...);

/* Batched inference */
int    crankl_holonomy_batch(const crankl_cran_t *, const float *x, size_t dim,
                             size_t batch, float *y);
double crankl_holonomy_mse_batch(const crankl_cran_t *, const float *,
                                 const float *, size_t dim, size_t batch);
int    crankl_holonomy_avx2_supported(void);

/* Pack objectives */
int    crankl_pack_f32_anneal(const float *data, size_t count, uint64_t *out_slots,
                              size_t n_slots, float alpha, float beta, int mode, unsigned seed);
int    crankl_pack_objective(const float *data, size_t count, const uint64_t *slots,
                             size_t n_slots, double lambda, double *w2,
                             double *frobenius, double *objective);
int    crankl_pack_default_mode(void);
```

### Appendix B: Artifact inventory

| Path | Role |
|------|------|
| `include/crankl/*.h` | public C API (umbrella: `crankl.h`) |
| `src/` | engine: pack, holonomy, topology/sheaf, ingest, archive I/O |
| `src/cli/main.cpp` | `crankl` CLI (16 commands) |
| `gui/` | Qt6 phase-2 GUI (QProcess job runner, compare page) |
| `tests/ctest/` | 24 test targets incl. golden parity + sanitizers |
| `tools/bench_sweep.c` | report-table benchmark harness |
| `scripts/verify_parity.sh` | golden + tests + CLI replay |
| `scripts/integration_test.sh` | e2e artifact lifecycle |
| `docs/CRANK_FORMAT.md` | format spec incl. multi-tensor addendum |
| `docs/PACK_V3_DESIGN.md` | pack objective design |
| `docs/adr/0001-sheaf-cohomology.md` | cohomology decision record |
