#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace crankl {
namespace pack {

constexpr size_t BLOCK_FLOATS = 64;
constexpr size_t BLOCK_DIM = 8;

size_t n_slots_from_count(size_t count);
void copy_weight_block(const float *data, size_t count, size_t slot_idx, float out[BLOCK_FLOATS]);

struct PersistencePair {
    float birth;
    float death;
};

std::vector<PersistencePair> persistence_diagram_1d(const float *data, size_t n);
float wasserstein_persistence(const std::vector<PersistencePair> &a,
                              const std::vector<PersistencePair> &b);
float spectral_range(const float *data, size_t n);

int fold_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots, float lambda,
             float mu);
int unfold_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count);
int unfold_f32_mode(const uint64_t *slots, size_t n_slots, float *out, size_t count, int mode);

} // namespace pack
} // namespace crankl
