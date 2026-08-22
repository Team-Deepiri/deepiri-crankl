#include "crankl/simd.h"
#include "internal_headers/simd.hpp"

extern "C" {

int crankl_has_avx2(void) {
    return crankl::simd::has_avx2() ? 1 : 0;
}

int crankl_holonomy_avx2_supported(void) {
    return crankl::simd::has_avx2() ? 1 : 0;
}

} // extern "C"
