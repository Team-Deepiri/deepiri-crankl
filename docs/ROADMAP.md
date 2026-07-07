# Crankle Roadmap

| Version | Deliverable |
|---------|-------------|
| v0.1 | Scaffold — GUCT core, `.cran` I/O, CLI, notebook parity |
| v0.2 | SIMD matmul, sheaf χ v2, cran metadata, diff/version CLI, anneal pack |
| v0.3 | Topological pack v2, safetensors ingest, loss-guided turn | **in progress** |
| v0.4 | Crankle Flow + decrank-unified finetune: inspect, compare, pipeline manifests, Maurer-Cartan turn |
| v1.0 | Production crank-turn finetune on LoRA-scale adapters at Deepiri scale |

## AI development flow (why this exists)

Crankle is infrastructure for how Deepiri builds and ships AI — not a side experiment.

| Dev flow stage | Crankle primitive |
|----------------|-------------------|
| Compress adapter / embedding deltas | `pack` → `.cran` |
| Lightweight finetune without full float soup | `turn` (symplectic anneal) |
| Compare two training runs / checkpoints | `diff`, `resonance` |
| Merge specialist adapters | `bind` |
| Roll back a finetune layer | `peel` |
| Run forward on compressed weights | `holonomy` |
| Ship artifacts with provenance | `.cran` metadata |

Works in finetuning pipelines, agent eval, vector tooling, and anywhere you move weights — same CLI, same format, no import coupling required.

## v0.3+ milestones

1. **Topological BO packing** — Wasserstein persistence objective, not just greedy anneal
2. **Safetensors / gguf ingest** — `crankle pack --input model.safetensors`
3. **Training pipeline** — `crankle turn` as a post-training step in standard Deepiri CI
4. **Checkpoint archaeology** — diff + stats in agent run logs

## v0.4 Flow milestones

1. **Archive metrics** — density, entropy, energy, depth envelope, β₁ proxy.
2. **Inspect** — machine-readable archive health for CI and eval logs.
3. **Compare** — checkpoint deltas with resonance + metric movement.
4. **Pipeline** — one command to pack → turn → emit `.cran` + manifest.
5. **Manifest discipline** — JSON artifacts are the stable handoff between training, eval, and agents.
