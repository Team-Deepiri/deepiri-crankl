# Next phase — design then ship

**Status:** v0.4 Flow + decrank-unified finetune is on `main` (inspect, compare, pipeline, `finetune`, peel stacks, safetensors).  
**Horizon:** close the remaining GUCT proxies and make crank-turn production-ready on LoRA-scale adapters (**v0.5 → v1.0**).

This doc splits work into a **novel design phase** (spec before code) and an **implementation phase** (ship against those specs). See [ROADMAP.md](ROADMAP.md), [IMPLEMENTATION.md](IMPLEMENTATION.md), and [V0.4_FINETUNE_PLAN.md](V0.4_FINETUNE_PLAN.md).

---

## Where we are

| Pillar | Today | Still proxy / incomplete |
|--------|-------|--------------------------|
| Clifford crank | Cl(3) product, decrank, resonance | Full 6-bivector blade use in hot paths |
| Sheaf | Restriction graph, β₁ / χ proxies | Real coboundary δ, H¹ cohomology |
| Symplectic Turn | Verlet + BCH + loss-guided / finetune | Task-loss callbacks wired into Deepiri CI |
| RG peel | Layer stacks in cran v2 + `peel` | Provenance-rich multi-layer archaeology |
| Persistent pack | 1D PD + W₂ anneal | Topological BO over trit complexes |
| Holonomy | Path-ordered Padé exp, blocked | AVX2 batched slots at LoRA width |
| Ingest | Raw f32, single-tensor safetensors | Multi-tensor safetensors, GGUF |
| Flow | inspect / compare / pipeline manifests | Tombstone + Helox post-train hooks |

**Outcome metric for the next phase:** a real LoRA safetensors adapter packs → finetunes → peels → diffs in CI with no “proxy” in sheaf or pack hot paths, and holonomy MSE within agreed ε of float baseline.

---

## Novel design phase (v0.5-D)

*Design first. No large kernel rewrites until the contracts below are written and reviewed.*

### D1 — Sheaf cohomology (replace proxies)

**Problem:** `sheaf_beta1_proxy` and χ are cycle-rank / alternating sums, not cohomology.

**Design deliverables:**

1. Spec for restriction sheaf on crank slots: stalks, restriction maps ρᵢⱼ, coboundary δ: C⁰ → C¹.
2. Definition of H⁰ / H¹ for a `.crank` archive; when resonance uses dim H¹ vs β₁ proxy.
3. Numerical contract: stability under peel, bind, and small trit surgery; golden notebook cases.
4. CLI / JSON surface: deprecate `beta1_proxy` naming once H¹ is default (`beta1` or `h1_dim`).

**Exit:** ADR + notebook golden cases accepted; API sketch in [API.md](API.md) draft section.

### D2 — Topological Bayesian pack

**Problem:** Pack anneal is greedy; GUCT target includes λ·W₂(PD) + μ·β₁ (soon μ·H¹) but no BO over trit complexes.

**Design deliverables:**

1. Search space: which trit / blade coordinates are free per 8×8 tile.
2. Surrogate (GP or cheaper) over persistence + Frobenius; acquisition for discrete crank edits.
3. Budget model: wall-clock vs W₂ improvement vs reconstruction ε.
4. Failure modes: when BO loses to current anneal (document defaults).

**Exit:** Pack v3 design note with acceptance thresholds for LoRA A/B matrices.

### D3 — Production `.crank` provenance & multi-tensor layout

**Problem:** Single-tensor pack is not enough for Deepiri LoRA / embedding shipping.

**Design deliverables:**

1. Multi-tensor safetensors → one archive or linked archive set; naming + checksum rules.
2. GGUF ingest sketch (tensor select, dtype, layout) — even if GGUF lands later in impl.
3. Manifest schema extension: parent run id, peels applied, finetune loss curve, sheaf H¹.
4. Versioning: cran/crank header flags for “cohomology metrics present.”

**Exit:** Format addendum (or CRANK_FORMAT v2 draft) + example `run.json` for Helox/Tombstone.

### D4 — Deepiri integration contract

**Problem:** CLI exists; training/eval systems do not yet own the loop.

**Design deliverables:**

1. Post-train hook contract for Helox: inputs, exit codes, manifest path.
2. Tombstone / eval harness: when to `compare` vs `diff` vs `resonance`; gate thresholds.
3. Optional C `crankl_loss_fn` task-loss callback profile for agent eval.
4. Security: size limits, untrusted safetensors/GGUF bounds (align with existing I/O limits).

**Exit:** One-page integration contract linked from [FLOW.md](FLOW.md).

---

## Implementation phase (v0.5-I → v1.0)

*Ship in dependency order. Design D1–D4 gate the corresponding impl buckets.*

### I1 — Ingest & scale (can start immediately)

| Item | Metric |
|------|--------|
| Multi-tensor `pack` from safetensors | All named LoRA tensors in one pipeline run |
| GGUF tensor select (after D3 sketch) | Smoke pack on a small GGUF |
| LoRA-scale bench | `bench_finetune.sh` / pack on ≥1 real adapter size |
| AVX2 batched holonomy | Throughput vs scalar baseline documented |

### I2 — Sheaf cohomology (after D1)

| Item | Metric |
|------|--------|
| Implement δ and H¹ | Proxy removed from inspect/compare JSON |
| Resonance uses cohomology | Golden parity + CI |
| Peel/bind preserve H¹ contract | Property tests |

### I3 — Pack v3 / topological BO (after D2)

| Item | Metric |
|------|--------|
| BO or staged anneal behind flag | W₂ + Frobenius vs v0.4 anneal on golden tiles |
| Default path stays fast | CI time budget not blown |

### I4 — CI & agent wiring (after D4)

| Item | Metric |
|------|--------|
| `post_train_crankl.sh` in Deepiri finetune CI | Artifact + manifest published per run |
| Eval harness checkpoint archaeology | Fail job on resonance / metric gate |
| Tombstone logs `compare --json` | Diff visible in run UI / logs |

### I5 — v1.0 production bar

| Criterion | Done when |
|-----------|-----------|
| Pack / unpack / holonomy agree | Decrank blocks; ε documented |
| Finetune on real LoRA | Recon + holonomy MSE drop on safetensors |
| No hot-path “proxy” | Sheaf + pack use real cohomology / PD objectives |
| Provenance | Manifest + layer stacks enough to peel and audit |
| Perf | Batched holonomy usable at adapter width |

---

## Suggested sequence

```text
Now          D3 sketch + I1 (multi-tensor, benches)
             D4 contract draft with Helox/Tombstone owners
Next         D1 sheaf ADR → I2
             D2 pack BO design → I3
Later        I4 CI gates → I5 v1.0 release
```

## Ownership (repo)

| Work | Primary touch |
|------|----------------|
| Design D1–D2 (math) | `docs/`, notebooks, `src/core/sheaf*`, `src/pack/*` |
| Design D3–D4 (product) | `docs/FLOW.md`, `docs/CRANK_FORMAT.md`, scripts |
| Impl I1–I5 | `libcrankl` + CLI + `scripts/*` + CI |

Reviewers for this phase doc: @aniakula (core / multivec), plus Flow/CI owners as D4 lands.
