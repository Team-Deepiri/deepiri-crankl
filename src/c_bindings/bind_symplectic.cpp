#include "crankl/symplectic.h"
#include "internal_headers/c_bindings.hpp"
#include "internal_headers/dynamics.hpp"
#include "internal_headers/holonomy.hpp"
#include "internal_headers/topology.hpp"

extern "C" {

int crankl_turn(uint64_t *word, double lr) {
    if (!word)
        return CRANKL_ERR_NULL;
    return crankl::symplectic_turn(*word, lr);
}

int crankl_turn_toward(uint64_t *word, double lr, const float *target, size_t target_len) {
    if (!word)
        return CRANKL_ERR_NULL;
    return crankl::symplectic_turn_toward(*word, lr, target, target_len);
}

int crankl_peel(uint64_t *word, uint32_t layers) {
    if (!word)
        return CRANKL_ERR_NULL;
    return crankl::rg_peel(*word, layers);
}

uint64_t crankl_bind(uint64_t a, uint64_t b) {
    return crankl::bind_cranks(a, b);
}

int crankl_finetune(uint64_t *slots, size_t n_slots, crankl_cran_t *cran_view,
                    const float *target_blocks, crankl_loss_fn task_loss, void *ctx, int steps,
                    double lr, double recon_weight, double task_weight) {
    if (!slots || n_slots == 0)
        return CRANKL_ERR_NULL;

    crankl_cran_t view{};
    if (cran_view)
        view = *cran_view;
    view.slots = slots;
    view.header.n_slots = n_slots;

    int rc = crankl::symplectic_finetune(slots, n_slots, &view, target_blocks, task_loss, ctx,
                                         steps, lr, recon_weight, task_weight);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_INVALID;
}

double crankl_holonomy_mse(const crankl_cran_t *cran, const float *calib_x, const float *calib_y,
                           size_t dim) {
    if (!cran || !calib_x || !calib_y || dim == 0)
        return 0.0;
    return crankl::holonomy::holonomy_mse(cran, calib_x, calib_y, dim);
}

int crankl_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                      uint32_t stack_depth, uint32_t layers) {
    if (!slots)
        return CRANKL_ERR_NULL;
    return crankl::rg_peel_stack(slots, n_slots, layer_stacks, stack_depth, layers);
}

} // extern "C"
