#pragma once

#include <cstddef>
#include <cstdint>

namespace crankl {

double sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other, size_t n_other);
int sheaf_beta1_proxy(const uint64_t *slots, size_t n);
int sheaf_h0_dim(const uint64_t *slots, size_t n);
int sheaf_h1_dim(const uint64_t *slots, size_t n);
int sheaf_cohomology(const uint64_t *slots, size_t n, int *h0_out, int *h1_out);

/* Reference path via explicit rank elimination (ADR 0001). Quadratic in
 * practice; kept for cross-validation against the theorem fast path in tests.
 * Returns -1 on null pointers. */
int sheaf_cohomology_reference(const uint64_t *slots, size_t n, int *h0_out, int *h1_out);

/* Cohomology with a caller-chosen edge-restriction threshold (default 1e-6).
 * Returns -1 on null pointers, -2 on non-finite or non-positive tol. */
int sheaf_cohomology_tol(const uint64_t *slots, size_t n, double edge_tol, int *h0_out,
                         int *h1_out);
double sheaf_resonance_h1(const uint64_t *slots, size_t n, const uint64_t *other, size_t n_other);

int rg_peel(uint64_t &word, uint32_t layers);
int rg_peel_stack(uint64_t *slots, size_t n_slots, const uint64_t *layer_stacks,
                  uint32_t stack_depth, uint32_t layers);
uint64_t bind_cranks(uint64_t a, uint64_t b);

} // namespace crankl
