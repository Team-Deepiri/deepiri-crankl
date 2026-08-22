#ifndef CRANKL_SHEAF_H
#define CRANKL_SHEAF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double crankl_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                              size_t n_other);

/*
 * Deprecated cycle-rank proxy. Superseded by crankl_sheaf_h1_dim: the proxy
 * only counts edges on the consecutive-pair path graph, so it is 0 for every
 * archive. Kept for backward compatibility only; use H0/H1 instead.
 */
int crankl_sheaf_beta1_proxy(const uint64_t *slots, size_t n);

/*
 * Real restriction-sheaf cohomology (ADR 0001): stalks are the 8 multivector
 * components of each slot, the coboundary delta_0: C0 -> C1 is the difference
 * of restriction projections over the window-2 slot graph, and
 *   H0 = ker(delta_0),  H1 = C1 / im(delta_0).
 * Trivial cases: n == 0 -> (0, 0); n == 1 -> h0 = 1, h1 = 0.
 * Returns 0 on success, -1 on NULL input.
 */
int crankl_sheaf_h0_dim(const uint64_t *slots, size_t n);
int crankl_sheaf_h1_dim(const uint64_t *slots, size_t n);
int crankl_sheaf_cohomology(const uint64_t *slots, size_t n, int *h0_out, int *h1_out);

/* Resonance over dim H1 (instead of the beta1 proxy) plus the coboundary term. */
double crankl_sheaf_resonance_h1(const uint64_t *slots, size_t n, const uint64_t *other,
                                 size_t n_other);

#ifdef __cplusplus
}
#endif

#endif
