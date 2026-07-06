#include "core/internal.hpp"

#include <cmath>

namespace crankle {

int trit_encode(int trit) {
    switch (trit) {
    case TRIT_ZERO:
        return 0;
    case TRIT_PLUS:
        return 1;
    case TRIT_MINUS:
        return 2;
    default:
        return 3;
    }
}

int trit_decode(int two_bits) {
    switch (two_bits & 3) {
    case 0:
        return TRIT_ZERO;
    case 1:
        return TRIT_PLUS;
    case 2:
        return TRIT_MINUS;
    default:
        return TRIT_ZERO;
    }
}

} // namespace crankle
