#include "core/internal.hpp"

#include <cmath>

namespace crankle {

static int quantize_trit(double x) {
    if (x > 0.33)
        return TRIT_PLUS;
    if (x < -0.33)
        return TRIT_MINUS;
    return TRIT_ZERO;
}

static double dequantize_trit(int t) {
    switch (t) {
    case TRIT_PLUS:
        return 1.0;
    case TRIT_MINUS:
        return -1.0;
    default:
        return 0.0;
    }
}

uint64_t pack_crank_word(const Multivector &mv, uint8_t depth, uint8_t flags) {
    uint64_t w = 0;
    int16_t scalar_fp = static_cast<int16_t>(mv.s * 256.0);
    w |= (static_cast<uint64_t>(static_cast<uint16_t>(scalar_fp)));

    auto pack_trit_field = [&](const double vals[], int n, int bit_offset) {
        for (int i = 0; i < n; ++i) {
            int enc = trit_encode(quantize_trit(vals[i]));
            w |= (static_cast<uint64_t>(enc & 3) << (bit_offset + i * 2));
        }
    };

    pack_trit_field(mv.v, 3, 16);
    pack_trit_field(mv.b, 3, 22);
  // trivector uses 6 bits at 28
    for (int i = 0; i < 3; ++i) {
        int enc = trit_encode(quantize_trit(mv.p * (i == 0 ? 1.0 : 0.0)));
        w |= (static_cast<uint64_t>(enc & 3) << (28 + i * 2));
    }
    w |= (static_cast<uint64_t>(depth) << 52);
    w |= (static_cast<uint64_t>(flags & 0xF) << 60);
    return w;
}

void unpack_crank_word(uint64_t word, Multivector &mv, uint8_t &depth_out) {
    int16_t scalar_fp = static_cast<int16_t>(word & 0xFFFF);
    mv.s = static_cast<double>(scalar_fp) / 256.0;

    auto unpack_trits = [&](double *vals, int n, int bit_offset) {
        for (int i = 0; i < n; ++i) {
            int enc = static_cast<int>((word >> (bit_offset + i * 2)) & 3);
            vals[i] = dequantize_trit(trit_decode(enc));
        }
    };

    unpack_trits(mv.v, 3, 16);
    unpack_trits(mv.b, 3, 22);
    int enc0 = static_cast<int>((word >> 28) & 3);
    mv.p = dequantize_trit(trit_decode(enc0));
    depth_out = static_cast<uint8_t>((word >> 52) & 0xFF);
}

void clifford_reversion(const Multivector &a, Multivector &out) {
    out.s = a.s;
    for (int i = 0; i < 3; ++i)
        out.v[i] = a.v[i];
    for (int i = 0; i < 3; ++i)
        out.b[i] = -a.b[i];
    out.p = -a.p;
}

// Cl(3) positive signature geometric product (reference implementation)
void clifford_product(const Multivector &a, const Multivector &b, Multivector &out) {
    const double e1 = a.v[0], e2 = a.v[1], e3 = a.v[2];
    const double e12 = a.b[0], e23 = a.b[1], e13 = a.b[2];
    const double e123 = a.p;

    const double f1 = b.v[0], f2 = b.v[1], f3 = b.v[2];
    const double f12 = b.b[0], f23 = b.b[1], f13 = b.b[2];
    const double f123 = b.p;

    out.s = a.s * b.s + e1 * f1 + e2 * f2 + e3 * f3 - e12 * f12 - e23 * f23 - e13 * f13 - e123 * f123;

    out.v[0] = a.s * f1 + b.s * e1 + e23 * f13 - e13 * f23 + e123 * f2 - e2 * f123;
    out.v[1] = a.s * f2 + b.s * e2 + e13 * f12 - e12 * f13 - e123 * f3 + e3 * f123;
    out.v[2] = a.s * f3 + b.s * e3 + e12 * f23 - e23 * f12 + e1 * f123 - e123 * f1;

    out.b[0] = a.s * f12 + b.s * e12 + e1 * f2 - e2 * f1 + e3 * f123 - e123 * f3;
    out.b[1] = a.s * f23 + b.s * e23 + e2 * f3 - e3 * f2 + e1 * f123 - e123 * f1;
    out.b[2] = a.s * f13 + b.s * e13 + e1 * f3 - e3 * f1 - e2 * f123 + e123 * f2;

    out.p = a.s * f123 + b.s * e123 + e12 * f23 + e23 * f12 + e13 * f13 + e1 * f2 * f3 + e2 * f3 * f1 +
            e3 * f1 * f2 - f12 * e23 - f23 * e13 - f13 * e12;
}

double clifford_resonance(uint64_t a, uint64_t b) {
    Multivector ma, mb, rev, prod;
    uint8_t da, db;
    unpack_crank_word(a, ma, da);
    unpack_crank_word(b, mb, db);
    (void)da;
    (void)db;
    clifford_reversion(ma, rev);
    clifford_product(rev, mb, prod);
    auto norm = [](const Multivector &m) {
        double n = m.s * m.s;
        for (int i = 0; i < 3; ++i) {
            n += m.v[i] * m.v[i];
            n += m.b[i] * m.b[i];
        }
        n += m.p * m.p;
        return std::sqrt(std::max(1e-12, n));
    };
    return prod.s / (norm(ma) * norm(mb));
}

void decrank_matrix(uint64_t word, std::array<double, 64> &out) {
    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    (void)depth;
    out.fill(0.0);
    // Build 8x8 operator from basis action on Cl(3) basis vectors
    Multivector basis[8];
    for (auto &b : basis) {
        b = {};
    }
    basis[0].s = 1.0;
    basis[1].v[0] = 1.0;
    basis[2].v[1] = 1.0;
    basis[3].v[2] = 1.0;
    basis[4].b[0] = 1.0;
    basis[5].b[1] = 1.0;
    basis[6].b[2] = 1.0;
    basis[7].p = 1.0;

    for (int col = 0; col < 8; ++col) {
        Multivector r;
        clifford_product(mv, basis[col], r);
        double vec[8] = {r.s, r.v[0], r.v[1], r.v[2], r.b[0], r.b[1], r.b[2], r.p};
        for (int row = 0; row < 8; ++row) {
            out[row * 8 + col] = vec[row];
        }
    }
}

} // namespace crankle
