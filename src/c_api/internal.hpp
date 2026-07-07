#pragma once

#include "crankle/errors.h"
#include "crankle/types.h"
#include "core/internal.hpp"

namespace crankle::capi {

inline crankle::Multivector mv_from_c(const crankle_multivector_t *mv) {
    crankle::Multivector m{};
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

inline void mv_to_c(const crankle::Multivector &m, crankle_multivector_t *out) {
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
    return p ? CRANKLE_OK : CRANKLE_ERR_NULL;
}

template <typename T>
inline int require_ptr(const T *p) {
    return p ? CRANKLE_OK : CRANKLE_ERR_NULL;
}

} // namespace crankle::capi
