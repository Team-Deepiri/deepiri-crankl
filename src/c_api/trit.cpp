#include "c_api/internal.hpp"
#include "crankle/trit.h"

extern "C" {

int crankle_trit_encode(int trit, uint8_t *out2bits) {
    if (crankle::capi::require_ptr(out2bits) != CRANKLE_OK)
        return CRANKLE_ERR_NULL;
    *out2bits = static_cast<uint8_t>(crankle::trit_encode(trit) & 3);
    return CRANKLE_OK;
}

int crankle_trit_decode(uint8_t two_bits) { return crankle::trit_decode(two_bits); }

} // extern "C"
