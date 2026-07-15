#include "core/internal.hpp"

#include <cmath>

namespace crankl {

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
    int16_t scalar_fp = static_cast<int16_t>(mv.scalar * 256.0);
    w |= (static_cast<uint64_t>(static_cast<uint16_t>(scalar_fp)));

    auto pack_trit_field = [&](const double vals[], int n, int bit_offset) {
        for (int i = 0; i < n; ++i) {
            int enc = trit_encode(quantize_trit(vals[i]));
            w |= (static_cast<uint64_t>(enc & 3) << (bit_offset + i * 2));
        }
    };

    pack_trit_field(mv.vec, 3, 16);
    pack_trit_field(mv.bivec, 3, 22);
    // trivector uses 6 bits at 28
    for (int i = 0; i < 3; ++i) {
        int enc = trit_encode(quantize_trit(mv.trivec * (i == 0 ? 1.0 : 0.0)));
        w |= (static_cast<uint64_t>(enc & 3) << (28 + i * 2));
    }
    w |= (static_cast<uint64_t>(depth) << 52);
    w |= (static_cast<uint64_t>(flags & 0xF) << 60);
    return w;
}

void unpack_crank_word(uint64_t word, Multivector &mv, uint8_t &depth_out) {
    int16_t scalar_fp = static_cast<int16_t>(word & 0xFFFF);
    mv.scalar = static_cast<double>(scalar_fp) / 256.0;

    auto unpack_trits = [&](double *vals, int n, int bit_offset) {
        for (int i = 0; i < n; ++i) {
            int enc = static_cast<int>((word >> (bit_offset + i * 2)) & 3);
            vals[i] = dequantize_trit(trit_decode(enc));
        }
    };

    unpack_trits(mv.vec, 3, 16);
    unpack_trits(mv.bivec, 3, 22);
    int enc0 = static_cast<int>((word >> 28) & 3);
    mv.trivec = dequantize_trit(trit_decode(enc0));
    depth_out = static_cast<uint8_t>((word >> 52) & 0xFF);
}

void clifford_reversion(const Multivector &a, Multivector &out) {
    out.scalar = a.scalar;
    for (int i = 0; i < 3; ++i)
        out.vec[i] = a.vec[i];
    for (int i = 0; i < 3; ++i)
        out.bivec[i] = -a.bivec[i];
    out.trivec = -a.trivec;
}

// Cl(3) positive signature geometric product (reference implementation)
void clifford_product(const Multivector &a, const Multivector &b, Multivector &out) {
    const double e1 = a.vec[0], e2 = a.vec[1], e3 = a.vec[2];
    const double e12 = a.bivec[0], e23 = a.bivec[1], e13 = a.bivec[2];
    const double e123 = a.trivec;

    const double f1 = b.vec[0], f2 = b.vec[1], f3 = b.vec[2];
    const double f12 = b.bivec[0], f23 = b.bivec[1], f13 = b.bivec[2];
    const double f123 = b.trivec;

    out.scalar = a.scalar * b.scalar + e1 * f1 + e2 * f2 + e3 * f3 - e12 * f12 - e23 * f23 -
                 e13 * f13 - e123 * f123;

    out.vec[0] = a.scalar * f1 + b.scalar * e1 + e23 * f13 - e13 * f23 + e123 * f2 - e2 * f123;
    out.vec[1] = a.scalar * f2 + b.scalar * e2 + e13 * f12 - e12 * f13 - e123 * f3 + e3 * f123;
    out.vec[2] = a.scalar * f3 + b.scalar * e3 + e12 * f23 - e23 * f12 + e1 * f123 - e123 * f1;

    out.bivec[0] = a.scalar * f12 + b.scalar * e12 + e1 * f2 - e2 * f1 + e3 * f123 - e123 * f3;
    out.bivec[1] = a.scalar * f23 + b.scalar * e23 + e2 * f3 - e3 * f2 + e1 * f123 - e123 * f1;
    out.bivec[2] = a.scalar * f13 + b.scalar * e13 + e1 * f3 - e3 * f1 - e2 * f123 + e123 * f2;

    out.trivec = a.scalar * f123 + b.scalar * e123 + e12 * f23 + e23 * f12 + e13 * f13 +
                 e1 * f2 * f3 + e2 * f3 * f1 + e3 * f1 * f2 - f12 * e23 - f23 * e13 - f13 * e12;
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
        double n = m.scalar * m.scalar;
        for (int i = 0; i < 3; ++i) {
            n += m.vec[i] * m.vec[i];
            n += m.bivec[i] * m.bivec[i];
        }
        n += m.trivec * m.trivec;
        return std::sqrt(std::max(1e-12, n));
    };
    return prod.scalar / (norm(ma) * norm(mb));
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
    basis[0].scalar = 1.0;
    basis[1].vec[0] = 1.0;
    basis[2].vec[1] = 1.0;
    basis[3].vec[2] = 1.0;
    basis[4].bivec[0] = 1.0;
    basis[5].bivec[1] = 1.0;
    basis[6].bivec[2] = 1.0;
    basis[7].trivec = 1.0;

    for (int col = 0; col < 8; ++col) {
        Multivector r;
        clifford_product(mv, basis[col], r);
        double vec[8] = {r.scalar,   r.vec[0],   r.vec[1],   r.vec[2],
                         r.bivec[0], r.bivec[1], r.bivec[2], r.trivec};
        for (int row = 0; row < 8; ++row) {
            out[row * 8 + col] = vec[row];
        }
    }
}

} // namespace crankl
