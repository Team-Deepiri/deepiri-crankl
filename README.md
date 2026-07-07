# Crankl

**Crankl** — low-level engine for compressing, finetuning, and comparing model weights in
Deepiri's AI development flow. GUCT math under the hood; `crankl` on the CLI.

Use it when you need to pack adapter deltas, anneal a finetune (`turn`), diff two checkpoints,
or run forward on compressed weights — in training, eval, or agent pipelines.

| Artifact | Name |
|----------|------|
| CLI | `crankl` |
| Library | `libcrankl` |
| Format | `.cran` |

## AI dev flow

```
train / finetune → crankl pack → .cran artifact
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
| Cheap symplectic anneal on cranks | `crankl turn` |
| Decrank-unified finetune loop | `crankl finetune` |
| Creating a full training artifact | `crankl pipeline --manifest run.json` |
| Auditing a `.cran` checkpoint | `crankl inspect --json` |
| Comparing two checkpoints | `crankl compare --json` |
| Debugging two agent runs | `crankl diff` + `resonance` |
| Merging specialist heads | `crankl bind` |
| Undoing a finetune layer | `crankl peel` |
| Inference on compressed weights | `crankl holonomy` |

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
crankl pack   --input weights.f32 --shape 8,8 -o adapter.cran
crankl unpack --input adapter.cran -o reconstructed.f32
crankl resonance a.cran b.cran [--mode clifford|sheaf|both]
crankl turn   --input adapter.cran --steps 100 --lr 0.01 -o tuned.cran
crankl finetune --input adapter.cran --target weights.f32 --steps 200 -o tuned.cran
crankl finetune --input adapter.cran --target weights.f32 --calib-x x.f32 --calib-y y.f32 -o tuned.cran
crankl peel   --input adapter.cran --layers 1 -o peeled.cran
crankl bind   a.cran b.cran -o merged.cran
crankl diff   a.cran b.cran
crankl holonomy --input adapter.cran --vector x.bin -o y.bin
crankl stats  adapter.cran
crankl verify adapter.cran
crankl inspect adapter.cran [--json]
crankl compare baseline.cran tuned.cran [--json]
crankl pipeline --input weights.f32 --steps 64 -o tuned.cran --manifest run.json
```

## Theory

See [docs/GUCT.md](docs/GUCT.md), [docs/FLOW.md](docs/FLOW.md), [docs/FINETUNE.md](docs/FINETUNE.md), and
[docs/ROADMAP.md](docs/ROADMAP.md).

## License

MIT — Team Deepiri
