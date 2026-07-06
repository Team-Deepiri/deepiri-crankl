#include "core/internal.hpp"

#include <cmath>

namespace crankle {

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
        mv.b[0] *= uv;
        mv.b[1] *= uv;
        mv.b[2] *= uv;
        mv.p *= uv;
        // IR scalar fixed-point drift.
        mv.s *= (1.0 - 0.05 * mu);
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

} // namespace crankle
