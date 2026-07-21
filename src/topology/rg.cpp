#include "internal_headers/algebra.hpp"
#include "internal_headers/topology.hpp"

#include <algorithm>
#include <cmath>

namespace crankl {

int rg_peel(uint64_t &word, uint32_t layers) {
    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);

    if (layers == 0)
        return 0;

    if (layers >= depth)
        depth = 1;
    else
        depth = static_cast<uint8_t>(depth - layers);

    // Wilsonian RG: integrate out UV modes (bivector + pseudoscalar) at scale Λ/layers.
    for (uint32_t l = 0; l < layers; ++l) {
        double mu = 1.0 / (1.0 + static_cast<double>(l + 1));
        double uv = mu * mu;
        mv.bivec[0] *= uv;
        mv.bivec[1] *= uv;
        mv.bivec[2] *= uv;
        mv.trivec *= uv;
        // IR scalar fixed-point drift.
        mv.scalar *= (1.0 - 0.05 * mu);
    }

    word = pack_crank_word(mv, depth, static_cast<uint8_t>(word >> 60));
    return 0;
}

uint64_t bind_cranks(uint64_t a, uint64_t b) {
    Multivector ma, mb, prod;
    uint8_t da, db;
    unpack_crank_word(a, ma, da);
    unpack_crank_word(b, mb, db);
    clifford_product(ma, mb, prod);
    uint8_t depth = static_cast<uint8_t>(std::min<int>(da + db, 255));
    return pack_crank_word(prod, depth, 0);
}

int rg_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                  uint32_t stack_depth, uint32_t layers_to_pop) {
    if (!slots || n_slots == 0)
        return -1;

    if (layer_stacks && stack_depth > 0 && layers_to_pop > 0) {
        uint32_t pop = std::min(layers_to_pop, stack_depth);
        uint32_t layer_idx = (pop >= stack_depth) ? 0 : (stack_depth - pop - 1);
        const uint64_t *layer = layer_stacks + static_cast<size_t>(layer_idx) * n_slots;
        for (size_t i = 0; i < n_slots; ++i)
            slots[i] = layer[i];
        return 0;
    }

    for (size_t i = 0; i < n_slots; ++i)
        rg_peel(slots[i], layers_to_pop);
    return 0;
}

} // namespace crankl
