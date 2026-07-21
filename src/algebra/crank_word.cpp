#include "internal_headers/algebra.hpp"

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
