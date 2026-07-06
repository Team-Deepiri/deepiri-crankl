#include "core/internal.hpp"
#include "core/simd.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace crankle {

static constexpr int N = 8;

static double mat_norm_inf(const double a[N * N]) {
    double m = 0.0;
    for (int i = 0; i < N * N; ++i)
        m = std::max(m, std::fabs(a[i]));
    return m;
}

void mat8_identity(double out[N * N]) {
    std::memset(out, 0, N * N * sizeof(double));
    for (int i = 0; i < N; ++i)
        out[i * N + i] = 1.0;
}

void mat8_add(const double a[N * N], const double b[N * N], double out[N * N]) {
    for (int i = 0; i < N * N; ++i)
        out[i] = a[i] + b[i];
}

void mat8_scale(const double a[N * N], double s, double out[N * N]) {
    for (int i = 0; i < N * N; ++i)
        out[i] = a[i] * s;
}

void mat8_mul(const double a[N * N], const double b[N * N], double out[N * N]) {
    if (simd::has_avx2()) {
        simd::mat8_mul_avx2(a, b, out);
        return;
    }
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            double sum = 0.0;
            for (int k = 0; k < N; ++k)
                sum += a[r * N + k] * b[k * N + c];
            out[r * N + c] = sum;
        }
    }
}

void mat8_vec(const double a[N * N], const double x[N], double out[N]) {
    for (int r = 0; r < N; ++r) {
        double sum = 0.0;
        for (int c = 0; c < N; ++c)
            sum += a[r * N + c] * x[c];
        out[r] = sum;
    }
}

void mat8_skew(const double a[N * N], double out[N * N]) {
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c)
            out[r * N + c] = 0.5 * (a[r * N + c] - a[c * N + r]);
    }
}

void mat8_sym(const double a[N * N], double out[N * N]) {
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c)
            out[r * N + c] = 0.5 * (a[r * N + c] + a[c * N + r]);
    }
}

// exp(M) via scaling-squaring + [3/3] Padé approximant.
void mat8_exp(const double a[N * N], double out[N * N]) {
    double scale = mat_norm_inf(a);
    int k = 0;
    double scaled[N * N];
    std::memcpy(scaled, a, sizeof(scaled));
    while (scale > 0.5 && k < 12) {
        for (int i = 0; i < N * N; ++i)
            scaled[i] *= 0.5;
        scale *= 0.5;
        ++k;
    }

    double M2[N * N], M3[N * N];
    mat8_mul(scaled, scaled, M2);
    mat8_mul(M2, scaled, M3);

    double I[N * N], c0I[N * N], c1M[N * N], c2M2[N * N], c3M3[N * N];
    double num[N * N], den[N * N], inv_den[N * N];
    mat8_identity(I);

    // [3/3] Padé: (I + 0.5M + M2/10 + M3/120) / (I - 0.5M + M2/10 - M3/120)
    mat8_scale(I, 1.0, c0I);
    mat8_scale(scaled, 0.5, c1M);
    mat8_scale(M2, 0.1, c2M2);
    mat8_scale(M3, 1.0 / 120.0, c3M3);

    mat8_add(c0I, c1M, num);
    mat8_add(num, c2M2, num);
    mat8_add(num, c3M3, num);

    mat8_scale(c1M, -1.0, c1M);
    mat8_scale(c3M3, -1.0, c3M3);
    mat8_add(c0I, c1M, den);
    mat8_add(den, c2M2, den);
    mat8_add(den, c3M3, den);

    // Small-matrix inverse via Gauss-Jordan (8x8).
    double aug[N * 2 * N];
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            aug[r * (2 * N) + c] = den[r * N + c];
            aug[r * (2 * N) + N + c] = (r == c) ? 1.0 : 0.0;
        }
    }
    for (int col = 0; col < N; ++col) {
        int pivot = col;
        for (int r = col + 1; r < N; ++r) {
            if (std::fabs(aug[r * (2 * N) + col]) > std::fabs(aug[pivot * (2 * N) + col]))
                pivot = r;
        }
        for (int c = 0; c < 2 * N; ++c)
            std::swap(aug[col * (2 * N) + c], aug[pivot * (2 * N) + c]);
        double div = aug[col * (2 * N) + col];
        if (std::fabs(div) < 1e-14) {
            mat8_identity(out);
            return;
        }
        for (int c = 0; c < 2 * N; ++c)
            aug[col * (2 * N) + c] /= div;
        for (int r = 0; r < N; ++r) {
            if (r == col)
                continue;
            double factor = aug[r * (2 * N) + col];
            for (int c = 0; c < 2 * N; ++c)
                aug[r * (2 * N) + c] -= factor * aug[col * (2 * N) + c];
        }
    }
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c)
            inv_den[r * N + c] = aug[r * (2 * N) + N + c];

    mat8_mul(num, inv_den, out);

    for (int i = 0; i < k; ++i) {
        double sq[N * N];
        mat8_mul(out, out, sq);
        std::memcpy(out, sq, sizeof(sq));
    }
}

// exp(i·γ·A) · x  (real x → real y): Re( (cos(γA) + i·sin(γA)) x ) with A skew-symmetric.
void mat8_exp_i_apply(const double a[N * N], double gamma, const double x[N], double y[N]) {
    double skew[N * N], scaled[N * N], cosM[N * N], sinM[N * N];
    mat8_skew(a, skew);
    mat8_scale(skew, gamma, scaled);

    double scaled2[N * N], scaled3[N * N];
    mat8_mul(scaled, scaled, scaled2);
    mat8_mul(scaled2, scaled, scaled3);

    double I[N * N];
    mat8_identity(I);

    // cos(M) ≈ I - M²/2 + M⁴/24 ; sin(M) ≈ M - M³/6  (good for small ||γ·skew||)
    double m2s[N * N], m4s[N * N], m3s[N * N];
    mat8_scale(scaled2, -0.5, m2s);
    mat8_mul(scaled2, scaled2, m4s);
    mat8_scale(m4s, 1.0 / 24.0, m4s);
    mat8_add(I, m2s, cosM);
    mat8_add(cosM, m4s, cosM);

    mat8_scale(scaled3, -1.0 / 6.0, m3s);
    mat8_add(scaled, m3s, sinM);

    double cx[N], sx[N], out[N];
    mat8_vec(cosM, x, cx);
    mat8_vec(sinM, x, sx);
    for (int i = 0; i < N; ++i)
        out[i] = cx[i]; // Re(exp(iγG)x) = cos(γG)x for skew G

    // Symmetric part: amplitude modulation via exp(γ·sym)
    double sym[N * N], exp_sym[N * N];
    mat8_sym(a, sym);
    mat8_scale(sym, gamma, scaled);
    mat8_exp(scaled, exp_sym);
    mat8_vec(exp_sym, out, y);
}

} // namespace crankle
