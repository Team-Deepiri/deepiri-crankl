#include "crankle/crankle.h"

#include <cstdio>

int main() {
    std::printf("avx2=%d\n", crankle_has_avx2());
    uint64_t words[2] = {0xFFFF0000FFFF0000ULL, 0x0000FFFF0000FFFFULL};
    // smoke: library loads and simd probe works
    if (crankle_has_avx2() < 0) {
        return 1;
    }
    std::printf("test_simd ok\n");
    return 0;
}
