#include "crankl/metrics.h"
#include "c_api/internal.hpp"

static void metrics_to_c(const crankl::ArchiveMetrics &m, crankl_archive_metrics_t *out) {
    out->n_slots = m.n_slots;
    out->depth_min = m.depth_min;
    out->depth_max = m.depth_max;
    out->scalar_mean = m.scalar_mean;
    out->scalar_abs_mean = m.scalar_abs_mean;
    out->trit_density = m.trit_density;
    out->trit_entropy = m.trit_entropy;
    out->clifford_energy = m.clifford_energy;
    out->beta1_proxy = m.beta1_proxy;
}

extern "C" {

int crankl_compute_archive_metrics(const uint64_t *slots, size_t n_slots,
                                    crankl_archive_metrics_t *out) {
    if (!slots || !out)
        return CRANKL_ERR_NULL;
    crankl::ArchiveMetrics m{};
    int rc = crankl::compute_archive_metrics(slots, n_slots, m);
    if (rc != 0)
        return CRANKL_ERR_INVALID;
    metrics_to_c(m, out);
    return CRANKL_OK;
}

int crankl_cran_compute_metrics(const crankl_cran_t *cran, crankl_archive_metrics_t *out) {
    if (!cran || !out)
        return CRANKL_ERR_NULL;
    return crankl_compute_archive_metrics(cran->slots, cran->header.n_slots, out);
}

} // extern "C"
