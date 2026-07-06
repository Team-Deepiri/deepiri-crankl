#include "crankle/crankle.h"

#include <cstdio>

int main() {
    uint8_t bits = 0;
    if (crankle_trit_encode(CRANKLE_TRIT_PLUS, &bits) != 0)
        return 1;
    if (crankle_trit_decode(bits) != CRANKLE_TRIT_PLUS)
        return 2;
    if (crankle_trit_encode(CRANKLE_TRIT_MINUS, &bits) != 0)
        return 3;
    if (crankle_trit_decode(bits) != CRANKLE_TRIT_MINUS)
        return 4;
    std::printf("test_trit ok\n");
    return 0;
}
