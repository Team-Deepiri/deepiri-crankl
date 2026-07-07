# GUCT Full Implementation Guide

## Do we need to train anything?

**No separate Crankl model.** Crankl is not a neural network you pretrain.

| Question | Answer |
|----------|--------|
| Train Crankl itself? | **No** — it's deterministic math + discrete search |
| Train a model *with* Crankl? | **Yes, in your existing stack** — Helox, PyTorch, etc. produce float weights; Crankl compresses them |
| What is `turn`? | Discrete symplectic annealing on crank space — optional **post-training** refinement, not backprop |
| When is loss needed? | Only if you want task-aware anneal (`turn --target`) or eval-driven checkpoint tuning (v0.4) |

**Typical Deepiri flow:**

```
finetune (floats)  →  crankl pack  →  .cran artifact
                              ↓
                    crankl turn --target (optional, reconstruction)
                              ↓
                    ship / eval / bind / peel
```

You train adapters as today. Crankl is the **compression + geometry layer** on top.

---

## Implementation status (v0.3)

| Pillar | Status | Gap to v1.0 |
|--------|--------|-------------|
| Clifford crank | **Done** — Cl(3) product, decrank, resonance | 6 bivector blades in 64-bit word (currently 3) |
| Sheaf | **Partial** — restriction graph, β₁ proxy | Real coboundary δ, H¹ cohomology |
| Symplectic Turn | **Partial** — Verlet + BCH + trit surgery | Loss-guided turn (v0.3), eval harness hook (v0.4) |
| RG peel | **Partial** — UV damp + depth decrement | Wilsonian layer stacks in `.cran` |
| Persistent pack | **Partial** — 1D PD + W₂ + anneal | Topological BO, full matrix PD |
| Holonomy | **Partial** — path-ordered exp(iγ·M) | Full Padé exp on 8×8, batched slots |
| Ingest | **v0.3** — raw f32, safetensors | GGUF, safetensors multi-tensor |
| Training hooks | **v0.4** | CI turn pipeline, Tombstone diff |

---

## Next steps (ordered)

### Phase A — v0.3 (this PR)

1. **Safetensors ingest** — `crankl pack --input adapter.safetensors --tensor lora_A`
2. **Loss-guided turn** — `crankl turn --target original.f32` minimizes reconstruction, not just H
3. **Topological pack v2** — Wasserstein between source vs decranked persistence diagrams
4. **Implementation doc + tests** — golden safetensors, turn-target integration

### Phase B — v0.4 (agent / training integration)

1. Wire `crankl turn` into finetune CI as post-step
2. `crankl diff` in Tombstone / eval harness logs
3. Optional downstream loss callback (C API) for task-aware turn
4. Checkpoint archaeology: resonance between run N and N+1

### Phase C — v1.0 (production)

1. LoRA-scale adapters end-to-end with metadata provenance
2. AVX2 batched holonomy matmul
3. Topological BO packing (Gaussian process over trit complexes)
4. Full sheaf cohomology resonance

---

## What “fully implemented” means

GUCT is **fully implemented** when:

- Any LoRA/safetensors adapter packs to `.cran` with PD preservation within tolerance
- `turn --target` recovers within ε of float baseline on eval set
- `holonomy` forward matches reference matmul within γ calibration
- `diff` + `resonance` drive CI decisions between training runs
- No pillar uses “proxy” in the hot path

We are at **~60%** — core math is real; ingest, loss-guided ops, and cohomology are the remaining work.
