# Crankl

[![Crankl Build and Test](https://github.com/Team-Deepiri/deepiri-crankl/actions/workflows/crankl-build-and-test.yml/badge.svg?branch=dev)](https://github.com/Team-Deepiri/deepiri-crankl/actions/workflows/crankl-build-and-test.yml)
[![CodeQL](https://github.com/Team-Deepiri/deepiri-crankl/actions/workflows/codeql.yml/badge.svg?branch=dev)](https://github.com/Team-Deepiri/deepiri-crankl/actions/workflows/codeql.yml)
[![Release](https://img.shields.io/github/v/release/Team-Deepiri/deepiri-crankl)](https://github.com/Team-Deepiri/deepiri-crankl/releases/latest)

**Crankl** — low-level engine for compressing, finetuning, and comparing model weights in
Deepiri's AI development flow. GUCT math under the hood; `crankl` on the CLI.

Use it when you need to pack adapter deltas, anneal a finetune (`turn`), diff two checkpoints,
or run forward on compressed weights — in training, eval, or agent pipelines.

| Artifact | Name |
|----------|------|
| CLI | `crankl` |
| Library | `libcrankl` |
| Format | `.crank` |

## AI dev flow

```
train / finetune → crankl pack → .crank artifact
                      ↓
              crankl turn (anneal)
                      ↓
         deploy / eval / agent loop
                      ↓
         crankl diff (what changed?)
```

| You are… | Reach for… |
|----------|------------|
| Shipping a LoRA / adapter | `crankl pack` |
| Packing a whole checkpoint (many tensors) | `crankl pack --multi` |
| Ingesting GGUF weights | `crankl pack --input m.gguf --tensor NAME` |
| Cheap symplectic anneal on cranks | `crankl turn` |
| Decrank-unified finetune loop | `crankl finetune` |
| Creating a full training artifact | `crankl pipeline --manifest run.json` |
| Auditing a `.crank` checkpoint | `crankl inspect --json` |
| Comparing two checkpoints | `crankl compare --json` |
| Debugging two agent runs | `crankl diff` + `resonance` |
| Merging specialist heads | `crankl bind` |
| Undoing a finetune layer | `crankl peel` |
| Inference on compressed weights | `crankl holonomy` (add `--batch N` for throughput) |

Install once, pipe from anything. No Python wheel required — shell out from Helox, Cyrex, Tombstone, or your own scripts.

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 scripts/export_golden.py   # generate parity headers
ctest --test-dir build --output-on-failure
bash scripts/verify_parity.sh    # golden + 11 tests + CLI smoke
bash scripts/integration_test.sh # pack → turn → peel → diff → holonomy
```

## What is verified

- **Cl(3) algebra** — `e₁²=1`, `e₁e₂=e₁₂`, `e₁₂²=-1` (`test_clifford_parity`)
- **Notebook parity** — `export_golden.py` → `clifford_cases.hpp` checked in CI
- **Pack roundtrip** — 64-float matrix → crank → reconstruct (bounded error)
- **Integration** — cran I/O, turn, holonomy, diff end-to-end

## Commands

```bash
crankl version
crankl pack   --input weights.f32 --shape 8,8 -o adapter.crank
crankl unpack --input adapter.crank -o reconstructed.f32
crankl resonance a.crank b.crank [--mode clifford|sheaf|both]
crankl turn   --input adapter.crank --steps 100 --lr 0.01 -o tuned.crank
crankl finetune --input adapter.crank --target weights.f32 --steps 200 -o tuned.crank
crankl finetune --input adapter.crank --target weights.f32 --calib-x x.f32 --calib-y y.f32 -o tuned.crank
crankl peel   --input adapter.crank --layers 1 -o peeled.crank
crankl bind   a.crank b.crank -o merged.crank
crankl diff   a.crank b.crank
crankl holonomy --input adapter.crank --vector x.bin -o y.bin
crankl stats  adapter.crank
crankl verify adapter.crank
crankl inspect adapter.crank [--json]
crankl compare baseline.crank tuned.crank [--json]
crankl pipeline --input weights.f32 --steps 64 -o tuned.crank --manifest run.json
```

## Theory

See [docs/GUCT.md](docs/GUCT.md), [docs/FLOW.md](docs/FLOW.md), [docs/FINETUNE.md](docs/FINETUNE.md),
[docs/ROADMAP.md](docs/ROADMAP.md), and [docs/NEXT_PHASE.md](docs/NEXT_PHASE.md) (v0.5 design + impl).


## License

MIT — Team Deepiri
