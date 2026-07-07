#ifndef CRANKLE_SYMPLECTIC_H
#define CRANKLE_SYMPLECTIC_H

#include "crankle/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankle_turn(uint64_t *word, double lr);
int crankle_turn_toward(uint64_t *word, double lr, const float *target, size_t target_len);
int crankle_peel(uint64_t *word, uint32_t layers);
uint64_t crankle_bind(uint64_t a, uint64_t b);

typedef double (*crankle_loss_fn)(const crankle_cran_t *cran, void *ctx);

int crankle_finetune(uint64_t *slots, size_t n_slots, crankle_cran_t *cran_view,
                     const float *target_blocks, crankle_loss_fn task_loss, void *ctx, int steps,
                     double lr, double recon_weight, double task_weight);

double crankle_holonomy_mse(const crankle_cran_t *cran, const float *calib_x,
                            const float *calib_y, size_t dim);

int crankle_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                       uint32_t stack_depth, uint32_t layers);

#ifdef __cplusplus
}
#endif

#endif
