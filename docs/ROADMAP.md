# Crankl Roadmap

| Version | Deliverable |
|---------|-------------|
| v0.1 | Scaffold — GUCT core, `.cran` I/O, CLI, notebook parity |
| v0.2 | SIMD matmul, sheaf χ v2, cran metadata, diff/version CLI, anneal pack |
| v0.3 | Topological pack v2, safetensors ingest, loss-guided turn | **done** |
| v0.4 | Crankl Flow + decrank-unified finetune: inspect, compare, pipeline manifests, Maurer-Cartan turn | **in progress** |
| v1.0 | Production crank-turn finetune on LoRA-scale adapters at Deepiri scale |

## AI development flow (why this exists)

Crankl is infrastructure for how Deepiri builds and ships AI — not a side experiment.

| Dev flow stage | Crankl primitive |
|----------------|-------------------|
| Compress adapter / embedding deltas | `pack` → `.cran` |
| Lightweight finetune without full float soup | `finetune` (Maurer-Cartan + holonomy task loss) |
| Cheap symplectic anneal on cranks | `turn` |
| Compare two training runs / checkpoints | `diff`, `resonance` |
| Merge specialist adapters | `bind` |
| Roll back a finetune layer | `peel` |
| Run forward on compressed weights | `holonomy` |
| Ship artifacts with provenance | `.cran` metadata |

Works in finetuning pipelines, agent eval, vector tooling, and anywhere you move weights — same CLI, same format, no import coupling required.

## v0.3+ milestones

1. **Topological BO packing** — Wasserstein persistence objective, not just greedy anneal
2. **Safetensors / gguf ingest** — `crankl pack --input model.safetensors`
3. **Training pipeline** — `crankl turn` as a post-training step in standard Deepiri CI
4. **Checkpoint archaeology** — diff + stats in agent run logs

## v0.4 Flow milestones

1. **Archive metrics** — density, entropy, energy, depth envelope, β₁ proxy.
2. **Inspect** — machine-readable archive health for CI and eval logs.
3. **Compare** — checkpoint deltas with resonance + metric movement.
4. **Pipeline** — one command to pack → turn → emit `.cran` + manifest.
5. **Manifest discipline** — JSON artifacts are the stable handoff between training, eval, and agents.

## v0.4 finetune milestones

1. **Decrank-unified pack** — Frobenius on `decrank_matrix(C)` per 8×8 tile.
2. **Maurer-Cartan turn** — BCH Lie-group update with finite-difference trit gradients.
3. **`crankl finetune`** — reconstruction + holonomy MSE, layer stack persistence.
4. **Peel stacks** — cran v2 layer history rollback via `peel --layers N`.
5. **Post-train hooks** — `bench_finetune.sh`, `post_train_crankl.sh`.
