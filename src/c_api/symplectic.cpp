#include "c_api/internal.hpp"
#include "crankle/symplectic.h"
#include "crankle_internal_api.hpp"

extern "C" {

int crankle_turn(uint64_t *word, double lr) {
    if (!word)
        return CRANKLE_ERR_NULL;
    return crankle::symplectic_turn(*word, lr);
}

int crankle_turn_toward(uint64_t *word, double lr, const float *target, size_t target_len) {
    if (!word)
        return CRANKLE_ERR_NULL;
    return crankle::symplectic_turn_toward(*word, lr, target, target_len);
}

int crankle_peel(uint64_t *word, uint32_t layers) {
    if (!word)
        return CRANKLE_ERR_NULL;
    return crankle::rg_peel(*word, layers);
}

uint64_t crankle_bind(uint64_t a, uint64_t b) { return crankle::bind_cranks(a, b); }

int crankle_finetune(uint64_t *slots, size_t n_slots, crankle_cran_t *cran_view,
                     const float *target_blocks, crankle_loss_fn task_loss, void *ctx, int steps,
                     double lr, double recon_weight, double task_weight) {
    if (!slots || n_slots == 0)
        return CRANKLE_ERR_NULL;

    crankle_cran_t view{};
    if (cran_view)
        view = *cran_view;
    view.slots = slots;
    view.header.n_slots = n_slots;

    int rc = crankle::symplectic_finetune(slots, n_slots, &view, target_blocks, task_loss, ctx,
                                          steps, lr, recon_weight, task_weight);
    return rc == 0 ? CRANKLE_OK : CRANKLE_ERR_INVALID;
}

double crankle_holonomy_mse(const crankle_cran_t *cran, const float *calib_x,
                            const float *calib_y, size_t dim) {
    if (!cran || !calib_x || !calib_y || dim == 0)
        return 0.0;
    return crankle::holonomy::holonomy_mse(cran, calib_x, calib_y, dim);
}

int crankle_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                       uint32_t stack_depth, uint32_t layers) {
    if (!slots)
        return CRANKLE_ERR_NULL;
    return crankle::rg_peel_stack(slots, n_slots, layer_stacks, stack_depth, layers);
}

} // extern "C"
