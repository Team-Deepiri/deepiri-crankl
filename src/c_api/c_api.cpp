#include "crankle/crankle.h"
#include "crankle_internal_api.hpp"
#include "core/internal.hpp"

#include <cstring>

using namespace crankle;

extern "C" {

int crankle_trit_encode(int trit, uint8_t *out2bits) {
    if (!out2bits)
        return -1;
    *out2bits = static_cast<uint8_t>(trit_encode(trit) & 3);
    return 0;
}

int crankle_trit_decode(uint8_t two_bits) { return trit_decode(two_bits); }

uint64_t crankle_crank_from_multivector(const crankle_multivector_t *mv, uint8_t depth) {
    Multivector m{};
    if (mv) {
        m.s = mv->s;
        for (int i = 0; i < 3; ++i) {
            m.v[i] = mv->v[i];
            m.b[i] = mv->b[i];
        }
        m.p = mv->p;
    }
    return pack_crank_word(m, depth, 0);
}

void crankle_crank_to_multivector(uint64_t word, crankle_multivector_t *mv, uint8_t *depth_out) {
    Multivector m;
    uint8_t d = 0;
    unpack_crank_word(word, m, d);
    if (mv) {
        mv->s = m.s;
        for (int i = 0; i < 3; ++i) {
            mv->v[i] = m.v[i];
            mv->b[i] = m.b[i];
        }
        mv->p = m.p;
    }
    if (depth_out)
        *depth_out = d;
}

void crankle_clifford_reversion(const crankle_multivector_t *a, crankle_multivector_t *out) {
    Multivector ma{}, mo{};
    if (a) {
        ma.s = a->s;
        for (int i = 0; i < 3; ++i) {
            ma.v[i] = a->v[i];
            ma.b[i] = a->b[i];
        }
        ma.p = a->p;
    }
    clifford_reversion(ma, mo);
    if (out) {
        out->s = mo.s;
        for (int i = 0; i < 3; ++i) {
            out->v[i] = mo.v[i];
            out->b[i] = mo.b[i];
        }
        out->p = mo.p;
    }
}

void crankle_clifford_product(const crankle_multivector_t *a, const crankle_multivector_t *b,
                              crankle_multivector_t *out) {
    Multivector ma{}, mb{}, mo{};
    if (a) {
        ma.s = a->s;
        for (int i = 0; i < 3; ++i) {
            ma.v[i] = a->v[i];
            ma.b[i] = a->b[i];
        }
        ma.p = a->p;
    }
    if (b) {
        mb.s = b->s;
        for (int i = 0; i < 3; ++i) {
            mb.v[i] = b->v[i];
            mb.b[i] = b->b[i];
        }
        mb.p = b->p;
    }
    clifford_product(ma, mb, mo);
    if (out) {
        out->s = mo.s;
        for (int i = 0; i < 3; ++i) {
            out->v[i] = mo.v[i];
            out->b[i] = mo.b[i];
        }
        out->p = mo.p;
    }
}

double crankle_clifford_resonance(uint64_t a, uint64_t b) { return clifford_resonance(a, b); }

void crankle_decrank_matrix(uint64_t word, double out8x8[64]) {
    std::array<double, 64> arr{};
    decrank_matrix(word, arr);
    std::memcpy(out8x8, arr.data(), 64 * sizeof(double));
}

double crankle_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                               size_t n_other) {
    return sheaf_resonance(slots, n, other, n_other);
}

int crankle_sheaf_beta1_proxy(const uint64_t *slots, size_t n) {
    return sheaf_beta1_proxy(slots, n);
}

int crankle_turn(uint64_t *word, double lr) {
    if (!word)
        return -1;
    return symplectic_turn(*word, lr);
}

int crankle_peel(uint64_t *word, uint32_t layers) {
    if (!word)
        return -1;
    return rg_peel(*word, layers);
}

uint64_t crankle_bind(uint64_t a, uint64_t b) { return bind_cranks(a, b); }

int crankle_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                     float lambda, float mu) {
    return pack::fold_f32(data, count, out_slots, n_slots, lambda, mu);
}

int crankle_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count) {
    return pack::unfold_f32(slots, n_slots, out, count);
}

int crankle_cran_write(const char *path, const crankle_cran_header_t *hdr, const uint64_t *slots,
                       const uint64_t *layer_stacks, const uint32_t *depths) {
    return io::write_cran(path, hdr, slots, layer_stacks, depths);
}

int crankle_cran_read(const char *path, crankle_cran_t *out) { return io::read_cran(path, out); }

void crankle_cran_close(crankle_cran_t *cran) { io::close_cran(cran); }

int crankle_cran_verify(const crankle_cran_t *cran) { return io::verify_cran(cran); }

int crankle_holonomy(const crankle_cran_t *cran, const float *x, size_t dim, float *y) {
    return holonomy::forward(cran, x, dim, y);
}

} // extern "C"
