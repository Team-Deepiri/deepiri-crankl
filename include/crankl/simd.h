#ifndef CRANKL_SIMD_H
#define CRANKL_SIMD_H

#ifdef __cplusplus
extern "C" {
#endif

int crankl_has_avx2(void);

/* 1 when the batched holonomy kernels are compiled with AVX2 (FMA) support,
 * 0 otherwise. Batched calls remain correct either way. */
int crankl_holonomy_avx2_supported(void);

#ifdef __cplusplus
}
#endif

#endif
