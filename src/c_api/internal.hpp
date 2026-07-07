#pragma once

#include "crankl/errors.h"
#include "crankl/types.h"
#include "core/internal.hpp"

namespace crankl::capi {

inline crankl::Multivector mv_from_c(const crankl_multivector_t *mv) {
    crankl::Multivector m{};
    if (!mv)
        return m;
    m.s = mv->s;
    for (int i = 0; i < 3; ++i) {
        m.v[i] = mv->v[i];
        m.b[i] = mv->b[i];
    }
    m.p = mv->p;
    return m;
}

inline void mv_to_c(const crankl::Multivector &m, crankl_multivector_t *out) {
    if (!out)
        return;
    out->s = m.s;
    for (int i = 0; i < 3; ++i) {
        out->v[i] = m.v[i];
        out->b[i] = m.b[i];
    }
    out->p = m.p;
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
