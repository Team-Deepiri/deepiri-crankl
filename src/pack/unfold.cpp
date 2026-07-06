#include "core/internal.hpp"

#include <algorithm>
#include <cstring>

namespace crankle {
namespace pack {

int unfold_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count) {
    if (!slots || !out)
        return -1;
  size_t max_elems = n_slots * 8;
    size_t n = std::min(count, max_elems);
    for (size_t i = 0; i < n; ++i)
        out[i] = 0.0f;

    for (size_t s = 0; s < n_slots; ++s) {
        Multivector mv;
        uint8_t depth;
        unpack_crank_word(slots[s], mv, depth);
        (void)depth;
        size_t base = s * 8;
        if (base < n)
            out[base] = static_cast<float>(mv.s);
        for (int i = 0; i < 3 && base + 1 + i < n; ++i)
            out[base + 1 + i] = static_cast<float>(mv.v[i]);
        for (int i = 0; i < 3 && base + 4 + i < n; ++i)
            out[base + 4 + i] = static_cast<float>(mv.b[i]);
        if (base + 7 < n)
            out[base + 7] = static_cast<float>(mv.p);
    }
    return 0;
}

} // namespace pack
} // namespace crankle
