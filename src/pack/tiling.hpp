#pragma once

#include <cstddef>

namespace crankle {
namespace pack {

constexpr size_t BLOCK_FLOATS = 64;
constexpr size_t BLOCK_DIM = 8;

size_t n_slots_from_count(size_t count);
void copy_weight_block(const float *data, size_t count, size_t slot_idx, float out[BLOCK_FLOATS]);

} // namespace pack
} // namespace crankle
