#pragma once

#include "crankl/types.h"

#include <cstddef>
#include <cstdint>

namespace crankl {

double decrank_frobenius_loss(uint64_t word, const float W[64]);

int symplectic_turn(uint64_t &word, double lr);
int symplectic_turn_toward(uint64_t &word, double lr, const float *target, size_t target_len);
int symplectic_turn_loss(uint64_t &word, double lr, double (*loss_fn)(uint64_t, void *), void *ctx);

typedef double (*crankl_loss_fn)(const crankl_cran_t *cran, void *ctx);
int symplectic_finetune(uint64_t *slots, size_t n_slots, crankl_cran_t *cran,
                        const float *target_blocks, crankl_loss_fn task_loss, void *task_ctx,
                        int steps, double lr, double recon_weight, double task_weight);

} // namespace crankl
