# Crank-turn finetune (v0.4)

Decrank-unified finetune integrates pack, Maurer-Cartan turn, and holonomy task loss into one
discrete Lie-group optimization loop — no float backprop through Crankl.

## Objective

Per slot \(i\) with target weight block \(W_i \in \mathbb{R}^{8\times8}\):

\[
J = w_{\text{recon}} \sum_i \|W_i - \text{decrank}(C_i)\|_F^2
  + w_{\text{task}} \cdot \text{holonomy\_MSE}(C, x_{\text{calib}}, y_{\text{calib}})
\]

Turn update uses finite-difference Lie gradients over trit generators, then BCH exponentiation:

\[
C_{\text{new}} = C_{\text{old}} \otimes \exp(\mathrm{d}L/\mathrm{d}C)
\]

## CLI

```bash
# Reconstruction-only finetune toward float target blocks
crankl finetune --input adapter.crank --target weights.f32 \
    --steps 200 --lr 0.02 -o tuned.crank

# Reconstruction + holonomy calibration loss
crankl finetune --input adapter.crank --target weights.f32 \
    --calib-x calib_in.f32 --calib-y calib_out.f32 \
    --recon-weight 1.0 --task-weight 0.1 \
    --steps 200 -o tuned.crank --json

# Peel finetune history (cran v2 layer stacks)
crankl peel --input tuned.crank --layers 1 -o rolled_back.crank
```

## Pack / unpack alignment

| Mode | Command | Output |
|------|---------|--------|
| Decrank (default) | `crankl unpack --input a.crank -o out.f32` | 64 floats per slot |
| Legacy coeffs | `crankl unpack --unpack-mode coeffs ...` | 8 multivector coeffs per slot |

Pack minimizes Frobenius error on `decrank_matrix(C)` — the same operator holonomy uses.

## C API

```c
double crankl_decrank_frobenius_loss(uint64_t word, const float block64[64]);

int crankl_finetune(uint64_t *slots, size_t n_slots, crankl_cran_t *cran_view,
                     const float *target_blocks, crankl_loss_fn task_loss, void *ctx,
                     int steps, double lr, double recon_weight, double task_weight);

double crankl_holonomy_mse(const crankl_cran_t *cran, const float *calib_x,
                            const float *calib_y, size_t dim);

int crankl_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                       uint32_t stack_depth, uint32_t layers);
```

## Automation

```bash
./scripts/bench_finetune.sh
./scripts/post_train_crankl.sh weights.f32 tuned.crank manifest.json
```

See [V0.4_FINETUNE_PLAN.md](V0.4_FINETUNE_PLAN.md) for the full design rationale.
