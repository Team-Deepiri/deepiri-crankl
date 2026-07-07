#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crankle {

constexpr int TRIT_ZERO = 0;
constexpr int TRIT_PLUS = 1;
constexpr int TRIT_MINUS = 2;

int trit_encode(int trit);
int trit_decode(int two_bits);

struct Multivector {
    double s = 0;
    double v[3] = {0, 0, 0};
    double b[3] = {0, 0, 0}; // e12, e23, e13
    double p = 0;              // e123
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

double sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other, size_t n_other);
int sheaf_beta1_proxy(const uint64_t *slots, size_t n);

int symplectic_turn(uint64_t &word, double lr);
int symplectic_turn_toward(uint64_t &word, double lr, const float *target, size_t target_len);
int rg_peel(uint64_t &word, uint32_t layers);
uint64_t bind_cranks(uint64_t a, uint64_t b);

size_t crank_diff_count(const uint64_t *a, const uint64_t *b, size_t n);
double crank_diff_hamming(const uint64_t *a, const uint64_t *b, size_t n);

struct ArchiveMetrics {
    uint64_t n_slots = 0;
    uint32_t depth_min = 0;
    uint32_t depth_max = 0;
    double scalar_mean = 0.0;
    double scalar_abs_mean = 0.0;
    double trit_density = 0.0;
    double trit_entropy = 0.0;
    double clifford_energy = 0.0;
    double beta1_proxy = 0.0;
};

int compute_archive_metrics(const uint64_t *slots, size_t n_slots, ArchiveMetrics &out);

} // namespace crankle
