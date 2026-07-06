#include "crankle/crankle.h"

#include <cassert>
#include <cstdio>

int main() {
    uint8_t bits = 0;
    assert(crankle_trit_encode(CRANKLE_TRIT_PLUS, &bits) == 0);
    assert(crankle_trit_decode(bits) == CRANKLE_TRIT_PLUS);
    assert(crankle_trit_encode(CRANKLE_TRIT_MINUS, &bits) == 0);
    assert(crankle_trit_decode(bits) == CRANKLE_TRIT_MINUS);
    std::printf("test_trit ok\n");
    return 0;
}
