#include "internal_headers/pack.hpp"

#include <algorithm>
#include <cstring>

namespace crankl {
namespace pack {

size_t n_slots_from_count(size_t count) {
    if (count == 0)
        return 1;
    return (count + BLOCK_FLOATS - 1) / BLOCK_FLOATS;
}

void copy_weight_block(const float *data, size_t count, size_t slot_idx, float out[BLOCK_FLOATS]) {
    std::memset(out, 0, BLOCK_FLOATS * sizeof(float));
    if (!data)
        return;
    size_t base = slot_idx * BLOCK_FLOATS;
    for (size_t i = 0; i < BLOCK_FLOATS && base + i < count; ++i)
        out[i] = data[base + i];
}

} // namespace pack
} // namespace crankl
