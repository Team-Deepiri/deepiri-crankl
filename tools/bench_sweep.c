/* Reproducible benchmark sweep for the v0.5 technical report.
 *
 * Produces the two experiment tables:
 *   T1 — holonomy forward at LoRA scale (4096x64): serial vs batched across batch sizes
 *   T2 — pack objective modes across seeds: time and reconstruction quality
 *
 * Deterministic inputs; every number is reproducible from source alone.
 */
#include "crankl/crankl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static float *make_lora_data(size_t count) {
    /* Deterministic pseudo-weights in [-3.9, 3.9]: i%251 pattern cycles phases
     * across 64-float blocks so no two blocks are identical. */
    float *d = malloc(count * sizeof(float));
    if (!d)
        exit(1);
    for (size_t i = 0; i < count; ++i)
        d[i] = (float)((int)(i % 251) - 125) / 32.0f;
    return d;
}

static void quality(const float *data, const float *out, size_t count, double *mean_fro,
                    double *worst_fro) {
    size_t blocks = count / 64;
    double sum = 0.0, worst = 0.0;
    for (size_t b = 0; b < blocks; ++b) {
        double fro = 0.0;
        for (size_t i = 0; i < 64; ++i) {
            double e = (double)out[b * 64 + i] - (double)data[b * 64 + i];
            fro += e * e;
        }
        sum += fro;
        if (fro > worst)
            worst = fro;
    }
    *mean_fro = sum / (double)blocks;
    *worst_fro = worst;
}

static void table1_holonomy(void) {
    const size_t lora_rows = 4096, dim = 64;
    const size_t count = lora_rows * dim;
    const size_t n_slots = crankl_pack_n_slots(count);
    const size_t batches[] = {1, 4, 16, 64, 256, 1024};

    float *data = make_lora_data(count);
    uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
    if (!slots || crankl_pack_f32(data, count, slots, n_slots, 0.1f, 0.01f) != 0)
        exit(2);

    crankl_cran_t cran = {0};
    cran.header.n_slots = (uint32_t)n_slots;
    cran.header.gamma = 0.5f;
    cran.slots = slots;

    printf("### T1: holonomy forward, LoRA %zux%zu (%zu slots), avx2=%d\n\n", lora_rows, dim,
           n_slots, crankl_holonomy_avx2_supported());
    printf("| batch | serial ms | batched ms | speedup | max abs diff |\n");
    printf("|------:|----------:|-----------:|--------:|-------------:|\n");

    for (size_t bi = 0; bi < sizeof(batches) / sizeof(batches[0]); ++bi) {
        size_t B = batches[bi];
        float *x = malloc(B * dim * sizeof(float));
        float *ys = malloc(B * dim * sizeof(float));
        float *yb = malloc(B * dim * sizeof(float));
        if (!x || !ys || !yb)
            exit(3);
        for (size_t i = 0; i < B * dim; ++i)
            x[i] = (float)((int)(i % 97) - 48) / 16.0f;

        int reps_s = B >= 256 ? 3 : 10;
        double t0 = now_sec();
        for (int r = 0; r < reps_s; ++r)
            for (size_t v = 0; v < B; ++v)
                crankl_holonomy(&cran, x + v * dim, dim, ys + v * dim);
        double t_serial = (now_sec() - t0) / reps_s;

        int reps_b = B >= 256 ? 30 : 100;
        t0 = now_sec();
        for (int r = 0; r < reps_b; ++r)
            crankl_holonomy_batch(&cran, x, dim, B, yb);
        double t_batch = (now_sec() - t0) / reps_b;

        double maxdiff = 0.0;
        for (size_t i = 0; i < B * dim; ++i) {
            double d = fabs((double)yb[i] - (double)ys[i]);
            if (d > maxdiff)
                maxdiff = d;
        }

        printf("| %zu | %.3f | %.4f | %.1fx | %.2e |\n", B, t_serial * 1e3, t_batch * 1e3,
               t_serial / t_batch, maxdiff);

        free(x);
        free(ys);
        free(yb);
    }
    free(data);
    free(slots);
}

static void table2_pack(void) {
    const size_t count = 4096 * 64;
    const size_t n_slots = crankl_pack_n_slots(count);
    const unsigned seeds[] = {0, 7, 42, 12345, 999};
    const char *names[] = {"legacy", "staged", "bo"};

    float *data = make_lora_data(count);
    uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
    float *out = malloc(count * sizeof(float));
    if (!slots || !out)
        exit(4);

    printf("\n### T2: pack modes, LoRA %zux%zu, 5 seeds\n\n", count / 64, (size_t)64);
    printf("| mode | mean ms | mean block-Fro | worst block-Fro | objective |\n");
    printf("|:-----|--------:|---------------:|----------------:|----------:|\n");

    for (int mode = CRANKL_PACK_MODE_LEGACY; mode <= CRANKL_PACK_MODE_BO; ++mode) {
        double t_sum = 0.0, mf_sum = 0.0, wf_max = 0.0, obj_last = 0.0;
        for (size_t si = 0; si < sizeof(seeds) / sizeof(seeds[0]); ++si) {
            memset(slots, 0, n_slots * sizeof(uint64_t));
            double t0 = now_sec();
            if (crankl_pack_f32_anneal(data, count, slots, n_slots, 1.0f, 1.0f, mode,
                                       seeds[si]) != 0)
                exit(5);
            t_sum += now_sec() - t0;

            if (crankl_unpack_f32(slots, n_slots, out, count) != 0)
                exit(6);
            double mf, wf;
            quality(data, out, count, &mf, &wf);
            mf_sum += mf;
            if (wf > wf_max)
                wf_max = wf;
            double w2 = 0.0, fro = 0.0;
            crankl_pack_objective(data, count, slots, n_slots, 1.0, &w2, &fro, &obj_last);
        }
        size_t ns = sizeof(seeds) / sizeof(seeds[0]);
        printf("| %s | %.1f | %.1f | %.1f | %.6g |\n", names[mode], t_sum / ns * 1e3,
               mf_sum / ns, wf_max, obj_last);
    }
    free(data);
    free(slots);
    free(out);
}

int main(void) {
    table1_holonomy();
    table2_pack();
    return 0;
}
