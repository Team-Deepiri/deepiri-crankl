# Crankle

**Crankle** — Grand Unified Crank Theory (GUCT) engine: Clifford multivector cranks,
symplectic turn annealing, sheaf resonance, and `.cran` archives.

| Artifact | Name |
|----------|------|
| CLI | `crankle` |
| Library | `libcrankle` |
| Format | `.cran` |

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
python3 scripts/export_golden.py   # generate parity headers
ctest --test-dir build --output-on-failure
bash scripts/verify_parity.sh    # golden + 11 tests + CLI smoke
bash scripts/integration_test.sh # full pack→turn→peel pipeline
```

## What is verified

- **Cl(3) algebra** — `e₁²=1`, `e₁e₂=e₁₂`, `e₁₂²=-1` (see `test_clifford_parity`)
- **Notebook parity** — Python `export_golden.py` generates `clifford_cases.hpp` compared in CI
- **Pack roundtrip** — 64-float matrix → crank → reconstruct (max err bounded)
- **Integration** — cran I/O, turn, holonomy, diff in one test

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

See [docs/GUCT.md](docs/GUCT.md). Runnable math: [notebooks/](notebooks/).

## License

MIT — Team Deepiri
