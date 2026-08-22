// LoRA-scale holonomy benchmark: serial per-vector forward vs batched forward
// with hoisted Padé exponentials and AVX2 matrix-vector applies.
#include "crankl/crankl.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void) {
    const size_t lora_rows = 4096; /* LoRA A-matrix: 4096 x 64 */
    const size_t dim = 64;
    const size_t count = lora_rows * dim;
    const size_t n_slots = crankl_pack_n_slots(count);
    const size_t batch = 256;

    float *data = malloc(count * sizeof(float));
    float *xb = malloc(batch * dim * sizeof(float));
    float *yb = malloc(batch * dim * sizeof(float));
    if (!data || !xb || !yb)
        return 1;
    for (size_t i = 0; i < count; ++i)
        data[i] = (float)((int)(i % 251) - 125) / 32.0f;
    for (size_t i = 0; i < batch * dim; ++i)
        xb[i] = (float)((int)(i % 97) - 48) / 16.0f;

    uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
    if (!slots || crankl_pack_f32(data, count, slots, n_slots, 0.5f, 1.0f) != 0)
        return 2;

    crankl_cran_t cran = {0};
    cran.header.n_slots = (uint32_t)n_slots;
    cran.header.gamma = 0.5f;
    cran.slots = slots;

    double t0 = now_sec();
    for (size_t v = 0; v < batch; ++v) {
        if (crankl_holonomy(&cran, xb + v * dim, dim, yb + v * dim) != 0)
            return 3;
    }
    double t_serial = now_sec() - t0;

    t0 = now_sec();
    for (int rep = 0; rep < 10; ++rep) {
        if (crankl_holonomy_batch(&cran, xb, dim, batch, yb) != 0)
            return 4;
    }
    double t_batch = (now_sec() - t0) / 10.0;

    printf("holonomy_bench lora=%zux%zu slots=%zu batch=%zu avx2=%d "
           "serial_ms=%.2f batch_ms=%.3f speedup=%.1fx\n",
           lora_rows, dim, n_slots, batch, crankl_holonomy_avx2_supported(), t_serial * 1000.0,
           t_batch * 1000.0, t_serial / t_batch);

    free(data);
    free(xb);
    free(yb);
    free(slots);
    return 0;
}
