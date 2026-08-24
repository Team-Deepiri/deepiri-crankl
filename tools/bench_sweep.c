/* Reproducible benchmark sweep for the v0.5 technical report.
 *
 * Produces four experiment tables:
 *   T1  — holonomy forward at LoRA scale (4096x64): serial vs batched across batch sizes
 *   T1b — holonomy forward across LoRA shapes at batch 1024
 *   T2  — pack objective modes across seeds: time and reconstruction quality
 *   T3  — sheaf h1 drift gate: cohomology response to injected checkpoint drift
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

static double relative_error(const float *ref, const float *out, size_t count) {
    size_t blocks = count / 64;
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double e = (double)out[i] - (double)ref[i];
        num += e * e;
        den += (double)ref[i] * (double)ref[i];
    }
    return den > 0.0 ? sqrt(num / den) : 0.0;
    (void)blocks;
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

static void table1b_shapes(void) {
    const size_t shapes[][2] = {{1024, 64}, {2048, 32}, {4096, 64}, {4096, 128}};
    const size_t B = 1024;

    printf("\n### T1b: holonomy forward across LoRA shapes, batch %zu, avx2=%d\n\n", B,
           crankl_holonomy_avx2_supported());
    printf("| shape | slots | serial ms | batched ms | speedup | max abs diff |\n");
    printf("|:------|------:|----------:|-----------:|--------:|-------------:|\n");

    for (size_t s = 0; s < sizeof(shapes) / sizeof(shapes[0]); ++s) {
        const size_t rows = shapes[s][0], dim = shapes[s][1];
        const size_t count = rows * dim;
        const size_t n_slots = crankl_pack_n_slots(count);

        float *data = make_lora_data(count);
        uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
        float *x = malloc(B * dim * sizeof(float));
        float *ys = malloc(B * dim * sizeof(float));
        float *yb = malloc(B * dim * sizeof(float));
        if (!data || !slots || !x || !ys || !yb ||
            crankl_pack_f32(data, count, slots, n_slots, 0.1f, 0.01f) != 0)
            exit(7);
        for (size_t i = 0; i < B * dim; ++i)
            x[i] = (float)((int)(i % 97) - 48) / 16.0f;

        crankl_cran_t cran = {0};
        cran.header.n_slots = (uint32_t)n_slots;
        cran.header.gamma = 0.5f;
        cran.slots = slots;

        double t0 = now_sec();
        for (size_t v = 0; v < B; ++v)
            crankl_holonomy(&cran, x + v * dim, dim, ys + v * dim);
        double t_serial = now_sec() - t0;

        int reps_b = 30;
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

        printf("| %zux%-3zu | %zu | %.2f | %.4f | %.1fx | %.2e |\n", rows, dim, n_slots,
               t_serial * 1e3, t_batch * 1e3, t_serial / t_batch, maxdiff);

        free(data);
        free(slots);
        free(x);
        free(ys);
        free(yb);
    }
}

static void table3_drift_gate(void) {
    const size_t count = 4096 * 64;
    const size_t n_slots = crankl_pack_n_slots(count);
    const size_t n_blocks = n_slots;
    const double noise_eps[] = {0.0,   3e-5, 1e-4,  3e-4,  1e-3,
                                3e-3,  1e-2, 3e-2};
    const double decay[] = {0.0, 0.1, 0.3, 0.5, 0.7, 0.9};
    const double collapse_pct[] = {0.0, 1.0, 5.0, 10.0, 25.0};

    float *base = make_lora_data(count);
    float *drift = malloc(count * sizeof(float));
    uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
    float *out = malloc(count * sizeof(float));
    if (!drift || !slots || !out)
        exit(8);

    printf("\n### T3a: sheaf gate under benign dense noise, LoRA 4096x64 (%zu slots)\n\n",
           n_slots);
    printf("| eps | h0 | h1 | mean block-Fro vs base |\n");
    printf("|----:|---:|---:|-----------------------:|\n");
    for (size_t e = 0; e < sizeof(noise_eps) / sizeof(noise_eps[0]); ++e) {
        const double eps = noise_eps[e];
        for (size_t i = 0; i < count; ++i)
            drift[i] = base[i] + (float)(eps * (((int)(i % 13) - 6) / 6.0));
        memset(slots, 0, n_slots * sizeof(uint64_t));
        if (crankl_pack_f32(drift, count, slots, n_slots, 0.1f, 0.01f) != 0)
            exit(9);
        int h0 = -1, h1 = -1;
        if (crankl_sheaf_cohomology(slots, n_slots, &h0, &h1) != 0)
            exit(10);
        if (crankl_unpack_f32(slots, n_slots, out, count) != 0)
            exit(11);
        double mf, wf;
        quality(drift, out, count, &mf, &wf);
        printf("| %.0e | %d | %d | %.2f |\n", eps, h0, h1, mf);
    }

    printf("\n### T3b: sheaf gate under row decay (all rows scaled by 1-delta)\n\n");
    printf("| delta | h0 | h1 | rel. recon. error vs base |\n");
    printf("|------:|---:|---:|--------------------------:|\n");
    for (size_t d = 0; d < sizeof(decay) / sizeof(decay[0]); ++d) {
        const double delta = decay[d];
        memcpy(drift, base, count * sizeof(float));
        for (size_t b = 0; b < n_blocks; ++b)
            for (size_t i = 0; i < 64; ++i)
                drift[b * 64 + i] *= (float)(1.0 - delta);
        memset(slots, 0, n_slots * sizeof(uint64_t));
        if (crankl_pack_f32(drift, count, slots, n_slots, 0.1f, 0.01f) != 0)
            exit(12);
        int h0 = -1, h1 = -1;
        if (crankl_sheaf_cohomology(slots, n_slots, &h0, &h1) != 0)
            exit(13);
        if (crankl_unpack_f32(slots, n_slots, out, count) != 0)
            exit(14);
        printf("| %.1f | %d | %d | %.4f |\n", delta, h0, h1,
               relative_error(base, out, count));
    }

    printf("\n### T3c: sheaf gate under mode collapse (k%% of blocks replaced by block 0)\n\n");
    printf("| pct | h0 | h1 | rel. recon. error vs base |\n");
    printf("|---:|---:|---:|--------------------------:|\n");
    for (size_t p = 0; p < sizeof(collapse_pct) / sizeof(collapse_pct[0]); ++p) {
        const size_t k = (size_t)(collapse_pct[p] / 100.0 * (double)n_blocks);
        memcpy(drift, base, count * sizeof(float));
        for (size_t b = 0; b < k; ++b) {
            /* Deterministic even spread across the archive. */
            size_t target = (b * n_blocks) / (k > 0 ? k : 1);
            memcpy(drift + target * 64, base, 64 * sizeof(float));
        }
        memset(slots, 0, n_slots * sizeof(uint64_t));
        if (crankl_pack_f32(drift, count, slots, n_slots, 0.1f, 0.01f) != 0)
            exit(15);
        int h0 = -1, h1 = -1;
        if (crankl_sheaf_cohomology(slots, n_slots, &h0, &h1) != 0)
            exit(16);
        if (crankl_unpack_f32(slots, n_slots, out, count) != 0)
            exit(17);
        printf("| %.0f | %d | %d | %.4f |\n", collapse_pct[p], h0, h1,
               relative_error(base, out, count));
    }
    free(base);
    free(drift);
    free(slots);
    free(out);
}


static void table4_trajectory(void) {
    const size_t count = 4096 * 64;
    const size_t n_slots = crankl_pack_n_slots(count);
    const int epochs = 12;

    float *base = make_lora_data(count);
    float *drift = malloc(count * sizeof(float));
    uint64_t *slots = malloc(n_slots * sizeof(uint64_t));
    float *out = malloc(count * sizeof(float));
    if (!drift || !slots || !out)
        exit(18);

    /* Baseline cohomology on the healthy archive. */
    memset(slots, 0, n_slots * sizeof(uint64_t));
    if (crankl_pack_f32(base, count, slots, n_slots, 0.1f, 0.01f) != 0)
        exit(19);
    int h0_base, h1_base;
    if (crankl_sheaf_cohomology(slots, n_slots, &h0_base, &h1_base) != 0)
        exit(20);
    const double alarm_h1 = 0.8 * (double)h1_base;

    printf("\n### T4: simulated finetune trajectory, gate alarm vs magnitude metrics\n\n");
    printf("base h0=%d h1=%d, alarm when h1 < %.0f (20%% band)\n\n", h0_base, h1_base,
           alarm_h1);
    printf("| epoch | row-scale | h0 | h1 | rel.err | alarm |\n");
    printf("|------:|----------:|---:|---:|--------:|:------|\n");

    int alarm_epoch = -1;
    double err_at_alarm = 0.0;
    for (int e = 0; e <= epochs; ++e) {
        const double keep = 1.0 - 0.075 * (double)e;
        memcpy(drift, base, count * sizeof(float));
        for (size_t i = 0; i < count; ++i)
            drift[i] *= (float)keep;
        memset(slots, 0, n_slots * sizeof(uint64_t));
        if (crankl_pack_f32(drift, count, slots, n_slots, 0.1f, 0.01f) != 0)
            exit(21);
        int h0, h1;
        if (crankl_sheaf_cohomology(slots, n_slots, &h0, &h1) != 0)
            exit(22);
        if (crankl_unpack_f32(slots, n_slots, out, count) != 0)
            exit(23);
        const double rel = relative_error(base, out, count);
        const int alarm = (double)h1 < alarm_h1;
        if (alarm && alarm_epoch < 0) {
            alarm_epoch = e;
            err_at_alarm = rel;
        }
        printf("| %d | %.3f | %d | %d | %.4f | %s |\n", e, keep, h0, h1, rel,
               alarm ? "**ALARM**" : "");
    }
    if (alarm_epoch >= 0)
        printf("\nGate alarmed at epoch %d with rel.err %.4f; terminal-state rel.err is "
               "%.4f.\n",
               alarm_epoch, err_at_alarm, relative_error(base, out, count));
    free(base);
    free(drift);
    free(slots);
    free(out);
}

int main(void) {
    table1_holonomy();
    table1b_shapes();
    table2_pack();
    table3_drift_gate();
    table4_trajectory();
    return 0;
}
