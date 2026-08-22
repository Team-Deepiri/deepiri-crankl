# Crankl Flow

Crankl Flow is the AI development layer above the GUCT kernels. It turns `.crank` from a
compressed weight container into a workflow artifact that can be inspected, compared, and dropped
into training/eval automation.

## Commands

```bash
crankl inspect adapter.crank
crankl inspect adapter.crank --json

crankl compare baseline.crank tuned.crank
crankl compare baseline.crank tuned.crank --json

crankl pipeline \
  --input weights.f32 \
  --target target.f32 \
  --steps 64 \
  --lr 0.03 \
  -o tuned.crank \
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

`inspect` also reports sheaf cohomology (`cohomology_h0`, `cohomology_h1`): h0 counts the
independent global sections carried by the slots, and h1 counts obstructions (cycles). A
finetune that stays in-distribution keeps h1 at its baseline; a spike means new cycles
appeared. Multi-tensor archives additionally list their tensor index with per-tensor xxh64
checksums.

## Why this matters

Crankl now has the minimum usable loop for AI development:

1. **Pack** raw adapter / embedding weights into `.crank`.
2. **Turn** cranks toward a target or anneal them without a target.
3. **Inspect** artifact health: density, entropy, energy, depth.
4. **Compare** two runs: diff, resonance, metric deltas.
5. **Manifest** the run so CI, eval harnesses, and agent logs can record what changed.

This is intentionally framework-neutral. Helox, Tombstone, Cyrex, or a shell script can all call the
same binary and consume the same `.crank` + JSON manifest.
