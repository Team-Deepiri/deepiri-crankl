// Pack benchmark across modes (legacy / staged / BO-lite) at LoRA scale.
#include "crankl/pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static const char *mode_name(int mode) {
    switch (mode) {
    case CRANKL_PACK_MODE_STAGED:
        return "staged";
    case CRANKL_PACK_MODE_BO:
        return "bo";
    default:
        return "legacy";
    }
}

int main(void) {
    const size_t count = 4096 * 64; /* LoRA A-matrix */
    const size_t n_slots = crankl_pack_n_slots(count);

    float *data = malloc(count * sizeof(float));
    uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
    float *out = malloc(count * sizeof(float));
    if (!data || !slots || !out)
        return 1;
    for (size_t i = 0; i < count; ++i)
        data[i] = (float)((int)(i % 251) - 125) / 32.0f;

    for (int mode = CRANKL_PACK_MODE_LEGACY; mode <= CRANKL_PACK_MODE_BO; ++mode) {
        memset(slots, 0, n_slots * sizeof(uint64_t));
        double t0 = now_sec();
        if (crankl_pack_f32_anneal(data, count, slots, n_slots, 1.0f, 1.0f, mode,
                                   42u + (unsigned)mode) != 0)
            return 2;
        double dt = now_sec() - t0;

        if (crankl_unpack_f32(slots, n_slots, out, count) != 0)
            return 3;
        double worst = 0.0, sum = 0.0;
        for (size_t b = 0; b < count / 64; ++b) {
            double fro = 0.0;
            for (size_t i = 0; i < 64; ++i) {
                double d = (double)out[b * 64 + i] - (double)data[b * 64 + i];
                fro += d * d;
            }
            sum += fro;
            if (fro > worst)
                worst = fro;
        }
        printf("pack_bench mode=%s elapsed_ms=%.1f mean_block_fro=%.1f worst_block_fro=%.1f\n",
               mode_name(mode), dt * 1000.0, sum / (double)(count / 64), worst);
    }

    free(data);
    free(slots);
    free(out);
    return 0;
}
