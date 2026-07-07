#include "crankl/crankl.h"

#include <cstdio>
#include <vector>

int main() {
    crankl_multivector_t a{};
    a.s = 0.5;
    a.v[0] = 1.0;
    a.b[1] = -1.0;
    crankl_multivector_t b{};
    b.s = -0.25;
    b.v[1] = 1.0;
    b.p = 1.0;

    uint64_t slots[2] = {crankl_crank_from_multivector(&a, 2),
                         crankl_crank_from_multivector(&b, 5)};
    crankl_archive_metrics_t m{};
    if (crankl_compute_archive_metrics(slots, 2, &m) != CRANKL_OK)
        return 1;
    if (m.n_slots != 2 || m.depth_min != 2 || m.depth_max != 5)
        return 2;
    if (m.trit_density <= 0.0 || m.trit_entropy <= 0.0 || m.clifford_energy <= 0.0)
        return 3;

    crankl_cran_header_t hdr{};
    hdr.n_slots = 2;
    hdr.depth_max = 5;
    hdr.gamma = 1.0f;
    const char *path = "/tmp/crankl_metrics.cran";
    if (crankl_cran_write(path, &hdr, slots, nullptr, nullptr) != CRANKL_OK)
        return 4;
    crankl_cran_t cran{};
    if (crankl_cran_read(path, &cran) != CRANKL_OK)
        return 5;
    crankl_archive_metrics_t cm{};
    if (crankl_cran_compute_metrics(&cran, &cm) != CRANKL_OK)
        return 6;
    crankl_cran_close(&cran);

    std::printf("test_metrics ok density=%f entropy=%f energy=%f\n", cm.trit_density,
                cm.trit_entropy, cm.clifford_energy);
    return 0;
}
