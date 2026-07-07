#include "c_api/internal.hpp"
#include "crankl/trit.h"

extern "C" {

int crankl_trit_encode(int trit, uint8_t *out2bits) {
    if (crankl::capi::require_ptr(out2bits) != CRANKL_OK)
        return CRANKL_ERR_NULL;
    *out2bits = static_cast<uint8_t>(crankl::trit_encode(trit) & 3);
    return CRANKL_OK;
}

int crankl_trit_decode(uint8_t two_bits) { return crankl::trit_decode(two_bits); }

} // extern "C"
