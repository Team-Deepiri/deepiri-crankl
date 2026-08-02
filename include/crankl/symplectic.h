#ifndef CRANKL_SYMPLECTIC_H
#define CRANKL_SYMPLECTIC_H

#include "crankl/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankl_turn(uint64_t *word, double lr);
int crankl_turn_toward(uint64_t *word, double lr, const float *target, size_t target_len);
int crankl_peel(uint64_t *word, uint32_t layers);
uint64_t crankl_bind(uint64_t a, uint64_t b);

typedef double (*crankl_loss_fn)(const crankl_cran_t *cran, void *ctx);

int crankl_finetune(uint64_t *slots, size_t n_slots, crankl_cran_t *cran_view,
                    const float *target_blocks, crankl_loss_fn task_loss, void *ctx, int steps,
                    double lr, double recon_weight, double task_weight);

double crankl_holonomy_mse(const crankl_cran_t *cran, const float *calib_x, const float *calib_y,
                           size_t dim);

int crankl_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                      uint32_t stack_depth, uint32_t layers);

#ifdef __cplusplus
}
#endif

#endif
