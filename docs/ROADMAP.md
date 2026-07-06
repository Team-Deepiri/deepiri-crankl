# Crankle Roadmap

| Version | Deliverable |
|---------|-------------|
| v0.1 | Scaffold — GUCT core, `.cran` I/O, CLI, notebook parity |
| v0.2 | SIMD matmul, sheaf χ v2, cran metadata, diff/version CLI, anneal pack |
| v0.3 | Topological BO packing, safetensors ingest |
| v0.4 | Optional Helox adapter (no hard coupling) |
| v1.0 | Production crank-turn finetune on LoRA-scale adapters |

## v0.2 milestones (in progress)

1. **SIMD** — AVX2 trit unpack batch + `mat8_mul` fast path
2. **Sheaf χ v2** — cycle-rank β₁ + coboundary restriction delta in resonance
3. **`.cran` metadata** — optional JSON footer (model name, source hash)
4. **CLI** — `version`, `diff` (trit-level crank delta between archives)
5. **Pack** — simulated annealing pass on fold objective J(C)
6. **Golden parity** — sheaf + holonomy reference vectors in CI
