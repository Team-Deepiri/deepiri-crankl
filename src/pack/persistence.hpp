#pragma once

#include <cstddef>
#include <vector>

namespace crankle {
namespace pack {

struct PersistencePair {
    float birth;
    float death;
};

std::vector<PersistencePair> persistence_diagram_1d(const float *data, size_t n);
float wasserstein_persistence(const std::vector<PersistencePair> &a,
                              const std::vector<PersistencePair> &b);
float spectral_range(const float *data, size_t n);

} // namespace pack
} // namespace crankle
