#include "c_api/internal.hpp"
#include "crankle/symplectic.h"

extern "C" {

int crankle_turn(uint64_t *word, double lr) {
    if (!word)
        return CRANKLE_ERR_NULL;
    return crankle::symplectic_turn(*word, lr);
}

int crankle_peel(uint64_t *word, uint32_t layers) {
    if (!word)
        return CRANKLE_ERR_NULL;
    return crankle::rg_peel(*word, layers);
}

uint64_t crankle_bind(uint64_t a, uint64_t b) { return crankle::bind_cranks(a, b); }

} // extern "C"
