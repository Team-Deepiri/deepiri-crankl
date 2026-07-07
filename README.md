# Crankle

**Crankle** — low-level engine for compressing, finetuning, and comparing model weights in
Deepiri's AI development flow. GUCT math under the hood; `crankle` on the CLI.

Use it when you need to pack adapter deltas, anneal a finetune (`turn`), diff two checkpoints,
or run forward on compressed weights — in training, eval, or agent pipelines.

| Artifact | Name |
|----------|------|
| CLI | `crankle` |
| Library | `libcrankle` |
| Format | `.cran` |

## AI dev flow

```
train / finetune → crankle pack → .cran artifact
                      ↓
              crankle turn (anneal)
                      ↓
         deploy / eval / agent loop
                      ↓
         crankle diff (what changed?)
```

| You are… | Reach for… |
|----------|------------|
| Shipping a LoRA / adapter | `crankle pack` |
| Cheap finetune pass on cranks | `crankle turn` |
| Debugging two agent runs | `crankle diff` + `resonance` |
| Merging specialist heads | `crankle bind` |
| Undoing a finetune layer | `crankle peel` |
| Inference on compressed weights | `crankle holonomy` |

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
crankle version
crankle pack   --input weights.f32 --shape 8,8 -o adapter.cran
crankle unpack --input adapter.cran -o reconstructed.f32
crankle resonance a.cran b.cran [--mode clifford|sheaf|both]
crankle turn   --input adapter.cran --steps 100 --lr 0.01 -o tuned.cran
crankle peel   --input adapter.cran --layers 1 -o peeled.cran
crankle bind   a.cran b.cran -o merged.cran
crankle diff   a.cran b.cran
crankle holonomy --input adapter.cran --vector x.bin -o y.bin
crankle stats  adapter.cran
crankle verify adapter.cran
```

## Theory

See [docs/GUCT.md](docs/GUCT.md) and [docs/ROADMAP.md](docs/ROADMAP.md).

## License

MIT — Team Deepiri
