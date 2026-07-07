#include "crankl/simd.h"
#include "core/simd.hpp"

extern "C" {

int crankl_has_avx2(void) { return crankl::simd::has_avx2() ? 1 : 0; }

} // extern "C"
