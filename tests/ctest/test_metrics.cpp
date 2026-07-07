#include "crankle/crankle.h"

#include <cstdio>
#include <vector>

int main() {
    crankle_multivector_t a{};
    a.s = 0.5;
    a.v[0] = 1.0;
    a.b[1] = -1.0;
    crankle_multivector_t b{};
    b.s = -0.25;
    b.v[1] = 1.0;
    b.p = 1.0;

    uint64_t slots[2] = {crankle_crank_from_multivector(&a, 2),
                         crankle_crank_from_multivector(&b, 5)};
    crankle_archive_metrics_t m{};
    if (crankle_compute_archive_metrics(slots, 2, &m) != CRANKLE_OK)
        return 1;
    if (m.n_slots != 2 || m.depth_min != 2 || m.depth_max != 5)
        return 2;
    if (m.trit_density <= 0.0 || m.trit_entropy <= 0.0 || m.clifford_energy <= 0.0)
        return 3;

    crankle_cran_header_t hdr{};
    hdr.n_slots = 2;
    hdr.depth_max = 5;
    hdr.gamma = 1.0f;
    const char *path = "/tmp/crankle_metrics.cran";
    if (crankle_cran_write(path, &hdr, slots, nullptr, nullptr) != CRANKLE_OK)
        return 4;
    crankle_cran_t cran{};
    if (crankle_cran_read(path, &cran) != CRANKLE_OK)
        return 5;
    crankle_archive_metrics_t cm{};
    if (crankle_cran_compute_metrics(&cran, &cm) != CRANKLE_OK)
        return 6;
    crankle_cran_close(&cran);

    std::printf("test_metrics ok density=%f entropy=%f energy=%f\n", cm.trit_density,
                cm.trit_entropy, cm.clifford_energy);
    return 0;
}
