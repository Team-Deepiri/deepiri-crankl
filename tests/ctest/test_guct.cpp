#include "crankl/crankl.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <thread>

static int expect_near(double a, double b, double eps, const char *msg) {
    if (std::fabs(a - b) > eps) {
        std::fprintf(stderr, "FAIL: %s (got %g want %g)\n", msg, a, b);
        return 1;
    }
    return 0;
}

int main() {
    int fails = 0;

    crankl_multivector_t mv{};
    mv.s = 0.5;
    mv.v[0] = 0.5;
    mv.b[1] = -0.5;
    uint64_t w = crankl_crank_from_multivector(&mv, 4);
    crankl_multivector_t before{}, after{};
    uint8_t d = 0;
    crankl_crank_to_multivector(w, &before, &d);
    double h0 = before.s * before.s;
    for (int i = 0; i < 3; ++i) {
        h0 += before.v[i] * before.v[i];
        h0 += before.b[i] * before.b[i];
    }
    h0 += before.p * before.p;

    crankl_turn(&w, 0.05);
    crankl_crank_to_multivector(w, &after, &d);
    double h1 = after.s * after.s;
    for (int i = 0; i < 3; ++i) {
        h1 += after.v[i] * after.v[i];
        h1 += after.b[i] * after.b[i];
    }
    h1 += after.p * after.p;
    fails += expect_near(h0, h1, 0.5, "turn Hamiltonian drift");

    crankl_multivector_t holo_mv{};
    holo_mv.b[0] = 1.0;
    holo_mv.v[2] = 0.5;
    uint64_t slot = crankl_crank_from_multivector(&holo_mv, 1);
    crankl_cran_header_t hdr{};
    hdr.n_slots = 1;
    hdr.gamma = 0.3f;
    std::string path = std::filesystem::temp_directory_path().string() + "/" + "crankl_test_holo.crank";
    if (crankl_cran_write(path.c_str(), &hdr, &slot, nullptr, nullptr) != 0)
        return 1;
    crankl_cran_t cran{};
    if (crankl_cran_read(path.c_str(), &cran) != 0)
        return 1;
    float x[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    float y[8] = {0};
    crankl_holonomy(&cran, x, 8, y);
    crankl_cran_close(&cran);
    if (std::fabs(y[0] - x[0]) < 1e-6) {
        std::fprintf(stderr, "FAIL: holonomy should rotate state\n");
        ++fails;
    }

    if (fails == 0)
        std::printf("test_guct ok\n");
    return fails;
}
