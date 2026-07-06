#include "crankle/crankle.h"

#include <cstdio>

int main() {
    uint64_t a[4] = {0x1000, 0x2000, 0x3000, 0x4000};
    uint64_t b[4] = {0x1000, 0x2001, 0x3000, 0x4000};
    size_t changed = crankle_crank_diff_count(a, b, 4);
    double ham = crankle_crank_diff_hamming(a, b, 4);
    if (changed != 1) {
        std::printf("expected 1 slot changed got %zu\n", changed);
        return 1;
    }
    std::printf("test_diff ok changed=%zu hamming=%f\n", changed, ham);
    return 0;
}
