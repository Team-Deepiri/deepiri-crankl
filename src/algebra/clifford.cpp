#include "internal_headers/algebra.hpp"

#include <cmath>

namespace crankl {

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

} // namespace crankl
