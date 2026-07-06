#include "core/internal.hpp"
#include "pack/persistence.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace crankle {
namespace pack {

int fold_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots, float lambda,
             float mu) {
    if (!data || !out_slots || n_slots == 0)
        return -1;

    for (size_t s = 0; s < n_slots; ++s) {
        Multivector mv;
        size_t base = s * 8;
        if (base < count)
            mv.s = data[base];
        for (int i = 0; i < 3 && base + 1 + i < count; ++i)
            mv.v[i] = data[base + 1 + i];
        for (int i = 0; i < 3 && base + 4 + i < count; ++i)
            mv.b[i] = data[base + 4 + i];
        if (base + 7 < count)
            mv.p = data[base + 7];

        size_t slice_len = std::min<size_t>(8, count > base ? count - base : 0);
        auto diagram = persistence_diagram_1d(data + base, slice_len);
        std::vector<PersistencePair> zero;
        float w2 = wasserstein_persistence(diagram, zero);
        float pen = lambda * w2 + mu * static_cast<float>(spectral_range(data + base, slice_len));
        mv.s -= static_cast<double>(pen) * 0.001;
        out_slots[s] = pack_crank_word(mv, 1, 0);
    }
    return 0;
}

} // namespace pack
} // namespace crankle
