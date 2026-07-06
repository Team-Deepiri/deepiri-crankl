#include "crankle/crankle.h"

#include <cstdio>

int main() {
    uint64_t slots[4] = {1, 2, 3, 4};
    int b1 = crankle_sheaf_beta1_proxy(slots, 4);
    std::printf("beta1=%d\n", b1);
    std::printf("test_sheaf ok\n");
    return 0;
}
