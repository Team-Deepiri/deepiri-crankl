#pragma once

#include "crankl/errors.h"
#include "crankl/types.h"
#include "internal_headers/algebra.hpp"

namespace crankl::capi {

inline crankl::Multivector mv_from_c(const crankl_multivector_t *mv) {
    crankl::Multivector m{};
    if (!mv)
        return m;
    m.scalar = mv->s;
    for (int i = 0; i < 3; ++i) {
        m.vec[i] = mv->v[i];
        m.bivec[i] = mv->b[i];
    }
    m.trivec = mv->p;
    return m;
}

inline void mv_to_c(const crankl::Multivector &m, crankl_multivector_t *out) {
    if (!out)
        return;
    out->s = m.scalar;
    for (int i = 0; i < 3; ++i) {
        out->v[i] = m.vec[i];
        out->b[i] = m.bivec[i];
    }
    out->p = m.trivec;
}

template <typename T>
inline int require_ptr(T *p) {
    return p ? CRANKL_OK : CRANKL_ERR_NULL;
}

template <typename T>
inline int require_ptr(const T *p) {
    return p ? CRANKL_OK : CRANKL_ERR_NULL;
}

} // namespace crankl::capi
