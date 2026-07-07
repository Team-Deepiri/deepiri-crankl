# Crankle Flow

Crankle Flow is the AI development layer above the GUCT kernels. It turns `.cran` from a
compressed weight container into a workflow artifact that can be inspected, compared, and dropped
into training/eval automation.

## Commands

```bash
crankle inspect adapter.cran
crankle inspect adapter.cran --json

crankle compare baseline.cran tuned.cran
crankle compare baseline.cran tuned.cran --json

crankle pipeline \
  --input weights.f32 \
  --target target.f32 \
  --steps 64 \
  --lr 0.03 \
  -o tuned.cran \
  --manifest run.json
```

## Metrics

`inspect` and `pipeline` produce the same archive metrics:

| Metric | Meaning |
|--------|---------|
| `n_slots` | crank slots in the archive |
| `depth_min`, `depth_max` | RG depth envelope |
| `scalar_mean` | scalar Clifford part average |
| `scalar_abs_mean` | average scalar magnitude |
| `trit_density` | non-zero ternary operator density |
| `trit_entropy` | Shannon entropy over `{-1,0,+1}` trits |
| `clifford_energy` | average multivector norm squared |
| `beta1_proxy` | sheaf cycle-rank proxy |

## Why this matters

Crankle now has the minimum usable loop for AI development:

1. **Pack** raw adapter / embedding weights into `.cran`.
2. **Turn** cranks toward a target or anneal them without a target.
3. **Inspect** artifact health: density, entropy, energy, depth.
4. **Compare** two runs: diff, resonance, metric deltas.
5. **Manifest** the run so CI, eval harnesses, and agent logs can record what changed.

This is intentionally framework-neutral. Helox, Tombstone, Cyrex, or a shell script can all call the
same binary and consume the same `.cran` + JSON manifest.
