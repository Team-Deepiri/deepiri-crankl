#pragma once

#include "crankl/types.h"

#include <array>
#include <cstdint>

namespace crankl {

constexpr int TRIT_ZERO = 0;
constexpr int TRIT_PLUS = 1;
constexpr int TRIT_MINUS = 2;

int trit_encode(int trit);
int trit_decode(int two_bits);

struct Multivector {
    double scalar = 0;
    double vec[3] = {0, 0, 0};
    double bivec[3] = {0, 0, 0}; // e12, e23, e13
    double trivec = 0;           // e123
};

uint64_t pack_crank_word(const Multivector &mv, uint8_t depth, uint8_t flags = 0);
void unpack_crank_word(uint64_t word, Multivector &mv, uint8_t &depth_out);

void clifford_reversion(const Multivector &a, Multivector &out);
void clifford_product(const Multivector &a, const Multivector &b, Multivector &out);
double clifford_resonance(uint64_t a, uint64_t b);

void decrank_matrix(uint64_t word, std::array<double, 64> &out);

void mat8_identity(double out[64]);
void mat8_mul(const double a[64], const double b[64], double out[64]);
void mat8_vec(const double a[64], const double x[8], double out[8]);
void mat8_exp(const double a[64], double out[64]);
void mat8_skew(const double a[64], double out[64]);
void mat8_sym(const double a[64], double out[64]);
void mat8_exp_i_apply(const double a[64], double gamma, const double x[8], double y[8]);

} // namespace crankl
