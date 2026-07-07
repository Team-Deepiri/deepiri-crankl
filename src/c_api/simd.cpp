#include "crankle/simd.h"
#include "core/simd.hpp"

extern "C" {

int crankle_has_avx2(void) { return crankle::simd::has_avx2() ? 1 : 0; }

} // extern "C"
