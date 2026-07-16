#include "crankl/crankl.h"

#include <cstdio>

int main() {
    uint8_t bits = 0;
    if (crankl_trit_encode(CRANKL_TRIT_PLUS, &bits) != 0)
        return 1;
    if (crankl_trit_decode(bits) != CRANKL_TRIT_PLUS)
        return 2;
    if (crankl_trit_encode(CRANKL_TRIT_MINUS, &bits) != 0)
        return 3;
    if (crankl_trit_decode(bits) != CRANKL_TRIT_MINUS)
        return 4;
    std::printf("test_trit ok\n");
    return 0;
}
