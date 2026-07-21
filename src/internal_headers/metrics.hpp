#pragma once

#include <cstddef>
#include <cstdint>

namespace crankl {

size_t crank_diff_count(const uint64_t *a, const uint64_t *b, size_t n);
double crank_diff_hamming(const uint64_t *a, const uint64_t *b, size_t n);

struct ArchiveMetrics {
    uint64_t n_slots = 0;
    uint32_t depth_min = 0;
    uint32_t depth_max = 0;
    double scalar_mean = 0.0;
    double scalar_abs_mean = 0.0;
    double trit_density = 0.0;
    double trit_entropy = 0.0;
    double clifford_energy = 0.0;
    double beta1_proxy = 0.0;
};

int compute_archive_metrics(const uint64_t *slots, size_t n_slots, ArchiveMetrics &out);

} // namespace crankl
