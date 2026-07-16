#ifndef CRANKL_METRICS_H
#define CRANKL_METRICS_H

#include "crankl/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crankl_archive_metrics {
    uint64_t n_slots;
    uint32_t depth_min;
    uint32_t depth_max;
    double scalar_mean;
    double scalar_abs_mean;
    double trit_density;
    double trit_entropy;
    double clifford_energy;
    double beta1_proxy;
} crankl_archive_metrics_t;

int crankl_compute_archive_metrics(const uint64_t *slots, size_t n_slots,
                                    crankl_archive_metrics_t *out);
int crankl_cran_compute_metrics(const crankl_cran_t *cran, crankl_archive_metrics_t *out);

#ifdef __cplusplus
}
#endif

#endif
