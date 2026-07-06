# deepiri-crankle

**Crankle** — Grand Unified Crank Theory (GUCT) engine for compressing model weights and
embeddings into stacked ternary Clifford operator cells, with discrete symplectic annealing
(**Turn**), RG depth stacking, and holographic forward passes.

| Artifact | Name |
|----------|------|
| CLI | `crankle` |
| Library | `libcrankle` |
| Format | `.cran` (Crankle Archive) |

## Quick start

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/crankle --help
```

## Commands

```bash
crankle pack   --input weights.f32 --shape 8,8 -o adapter.cran
crankle unpack --input adapter.cran -o reconstructed.f32
crankle resonance a.cran b.cran [--mode clifford|sheaf|both]
crankle turn   --input adapter.cran --steps 100 --lr 0.01 -o tuned.cran
crankle peel   --input adapter.cran --layers 1 -o peeled.cran
crankle bind   a.cran b.cran -o merged.cran
crankle holonomy --input adapter.cran --vector x.bin -o y.bin
crankle stats  adapter.cran
crankle verify adapter.cran
```

## Theory

See [docs/GUCT.md](docs/GUCT.md). Runnable math lives in [notebooks/](notebooks/).

## License

MIT — Team Deepiri
