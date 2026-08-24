#include "crankl/crankl.h"
#include "crankl/version.h"
#include "crankl/crankl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

static void usage() {
    std::cout << "crankl v" << CRANKL_VERSION_STRING << " — Grand Unified Crank Theory engine\n"
              << "Usage: crankl <command> [options]\n"
              << "Commands: pack, unpack, resonance, turn, finetune, peel, bind, holonomy, stats,\n"
              << "          verify, diff, inspect, compare, pipeline, version\n"
              << "pack: --input f32|.safetensors|.gguf [--tensor NAME] [--multi]\n"
              << "      [--pack-mode legacy|staged|bo] [--manifest path] [-o out.crank]\n"
              << "holonomy: --input a.crank --vector x.f32 [--batch N] -o y.f32\n";
}

static std::vector<float> read_f32(const char *path, size_t &count) {
    FILE *f = std::fopen(path, "rb");
    if (!f)
        return {};
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return {};
    }
    long sz = std::ftell(f);
    /* Matches the library's ingest payload ceiling: refuse absurd inputs before
     * allocating. Keep in sync with archive.hpp CRANKL_MAX_FLOAT_BYTES. */
    constexpr size_t kMaxInputBytes = 256u << 20;
    if (sz < 0 || static_cast<size_t>(sz) > kMaxInputBytes) {
        std::fclose(f);
        return {};
    }
    if (std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return {};
    }
    count = static_cast<size_t>(sz / sizeof(float));
    if (count == 0 || count * sizeof(float) > static_cast<size_t>(sz)) {
        std::fclose(f);
        return {};
    }
    std::vector<float> data(count);
    size_t got = std::fread(data.data(), sizeof(float), count, f);
    std::fclose(f);
    if (got != count)
        return {};
    return data;
}

/* Reads a named tensor through the public ingest API so the CLI consumes
 * exactly the surface downstream users have. Returns empty vector on error. */
static std::vector<float> read_tensor_f32(const char *path, const char *tensor) {
    float *buf = nullptr;
    size_t n = 0;
    std::vector<float> data;
    if (crankl_safetensors_read_f32(path, tensor, &buf, &n) != CRANKL_OK)
        return data;
    data.assign(buf, buf + n);
    free(buf);
    return data;
}


static int write_f32(const char *path, const float *data, size_t count) {
    FILE *f = std::fopen(path, "wb");
    if (!f)
        return -1;
    std::fwrite(data, sizeof(float), count, f);
    std::fclose(f);
    return 0;
}

static int load_cran_slots(const char *path, std::vector<uint64_t> &slots, crankl_cran_t &cran) {
    if (crankl_cran_read(path, &cran) != 0)
        return -1;
    slots.assign(cran.slots, cran.slots + cran.header.n_slots);
    return 0;
}

static const char *bool_text(bool v) {
    return v ? "true" : "false";
}

static void print_metrics_text(const crankl_archive_metrics_t &m) {
    std::cout << "n_slots=" << m.n_slots << "\n"
              << "depth_min=" << m.depth_min << "\n"
              << "depth_max=" << m.depth_max << "\n"
              << "scalar_mean=" << m.scalar_mean << "\n"
              << "scalar_abs_mean=" << m.scalar_abs_mean << "\n"
              << "trit_density=" << m.trit_density << "\n"
              << "trit_entropy=" << m.trit_entropy << "\n"
              << "clifford_energy=" << m.clifford_energy << "\n"
              << "beta1_proxy=" << m.beta1_proxy << "\n";
}

static void print_metrics_json(const crankl_archive_metrics_t &m) {
    std::cout << "{" << "\"n_slots\":" << m.n_slots << "," << "\"depth_min\":" << m.depth_min << ","
              << "\"depth_max\":" << m.depth_max << "," << "\"scalar_mean\":" << m.scalar_mean
              << "," << "\"scalar_abs_mean\":" << m.scalar_abs_mean << ","
              << "\"trit_density\":" << m.trit_density << ","
              << "\"trit_entropy\":" << m.trit_entropy << ","
              << "\"clifford_energy\":" << m.clifford_energy << ","
              << "\"beta1_proxy\":" << m.beta1_proxy << "}";
}

static int write_manifest(const char *path, const char *input, const char *output, int steps,
                          double lr, const crankl_archive_metrics_t &metrics) {
    if (!path)
        return 0;
    FILE *f = std::fopen(path, "wb");
    if (!f)
        return 1;
    std::time_t now = std::time(nullptr);
    std::fprintf(f,
                 "{\n"
                 "  \"tool\": \"crankl\",\n"
                 "  \"version\": \"%s\",\n"
                 "  \"created_unix\": %lld,\n"
                 "  \"input\": \"%s\",\n"
                 "  \"output\": \"%s\",\n"
                 "  \"steps\": %d,\n"
                 "  \"lr\": %.12g,\n"
                 "  \"metrics\": {\n"
                 "    \"n_slots\": %llu,\n"
                 "    \"trit_density\": %.12g,\n"
                 "    \"trit_entropy\": %.12g,\n"
                 "    \"clifford_energy\": %.12g,\n"
                 "    \"beta1_proxy\": %.12g\n"
                 "  }\n"
                 "}\n",
                 CRANKL_VERSION_STRING, static_cast<long long>(now), input ? input : "",
                 output ? output : "", steps, lr, static_cast<unsigned long long>(metrics.n_slots),
                 metrics.trit_density, metrics.trit_entropy, metrics.clifford_energy,
                 metrics.beta1_proxy);
    std::fclose(f);
    return 0;
}

static int parse_pack_mode(const char *m, int &mode) {
    if (std::strcmp(m, "legacy") == 0)
        mode = CRANKL_PACK_MODE_LEGACY;
    else if (std::strcmp(m, "staged") == 0)
        mode = CRANKL_PACK_MODE_STAGED;
    else if (std::strcmp(m, "bo") == 0)
        mode = CRANKL_PACK_MODE_BO;
    else
        return -1;
    return 0;
}

static bool has_extension(const char *path, const char *ext) {
    size_t len = std::strlen(path), elen = std::strlen(ext);
    return len > elen && std::strcmp(path + len - elen, ext) == 0;
}

static int cmd_pack(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr, *tensor = nullptr, *manifest = nullptr;
    size_t n_slots = 8;
    int mode = crankl_pack_default_mode();
    bool multi = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--tensor") == 0 && i + 1 < argc)
            tensor = argv[++i];
        else if (std::strcmp(argv[i], "--manifest") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else if (std::strcmp(argv[i], "--multi") == 0)
            multi = true;
        else if (std::strcmp(argv[i], "--pack-mode") == 0 && i + 1 < argc) {
            if (parse_pack_mode(argv[++i], mode) != 0) {
                std::fprintf(stderr, "pack: unknown --pack-mode (legacy|staged|bo)\n");
                return 1;
            }
        } else if (std::strcmp(argv[i], "--shape") == 0 && i + 1 < argc) {
            n_slots = static_cast<size_t>(std::atoi(argv[++i]));
        }
    }
    if (!input || !output)
        return 1;

    // Multi-tensor and GGUF ingest bypass the raw-f32 path entirely.
    if (multi) {
        int rc = crankl_pack_safetensors_multi(input, output, manifest, 1.0f, 1.0f);
        if (rc != 0) {
            std::fprintf(stderr, "pack: multi-tensor pack failed (%d)\n", rc);
            return 2;
        }
        std::cout << "packed_multi=" << output << "\n";
        return 0;
    }
    if (has_extension(input, ".gguf")) {
        if (!tensor) {
            std::fprintf(stderr, "pack: .gguf input requires --tensor NAME\n");
            return 1;
        }
        if (crankl_pack_gguf_f32(input, tensor, output) != 0)
            return 2;
        std::cout << "packed_gguf=" << output << " tensor=" << tensor << "\n";
        return 0;
    }

    std::vector<float> data;
    size_t count = 0;

    if (tensor) {
        data = read_tensor_f32(input, tensor);
        if (data.empty())
            return 2;
        count = data.size();
    } else {
        data = read_f32(input, count);
        if (data.empty())
            return 2;
    }

    if (n_slots == 8 && count > 0)
        n_slots = crankl_pack_n_slots(count);

    std::vector<uint64_t> slots(n_slots);
    if (mode == CRANKL_PACK_MODE_LEGACY)
        crankl_pack_f32(data.data(), count, slots.data(), n_slots, 0.1f, 0.01f);
    else if (crankl_pack_f32_anneal(data.data(), count, slots.data(), n_slots, 1.0f, 1.0f, mode,
                                    7u) != 0)
        return 3;
    crankl_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;
    return crankl_cran_write(output, &hdr, slots.data(), nullptr, nullptr);
}

static int cmd_unpack(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr;
    int mode = CRANKL_UNPACK_DECRANK;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--unpack-mode") == 0 && i + 1 < argc) {
            const char *m = argv[++i];
            mode = std::strcmp(m, "coeffs") == 0 ? CRANKL_UNPACK_COEFFS : CRANKL_UNPACK_DECRANK;
        }
    }
    if (!input || !output)
        return 1;
    crankl_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;
    size_t out_count =
        mode == CRANKL_UNPACK_COEFFS ? slots.size() * 8 : slots.size() * CRANKL_BLOCK_FLOATS;
    std::vector<float> out(out_count);
    crankl_unpack_f32_mode(slots.data(), slots.size(), out.data(), out.size(), mode);
    crankl_cran_close(&cran);
    return write_f32(output, out.data(), out.size());
}

static int cmd_resonance(int argc, char **argv) {
    if (argc < 4)
        return 1;
    const char *mode = "both";
    for (int i = 4; i < argc; ++i) {
        if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            mode = argv[++i];
    }
    crankl_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;
    if (std::strcmp(mode, "clifford") == 0 || std::strcmp(mode, "both") == 0) {
        double r = 0;
        size_t n = std::min(sa.size(), sb.size());
        for (size_t i = 0; i < n; ++i)
            r += crankl_clifford_resonance(sa[i], sb[i]);
        if (n)
            r /= static_cast<double>(n);
        std::cout << "clifford_resonance=" << r << "\n";
    }
    if (std::strcmp(mode, "sheaf") == 0 || std::strcmp(mode, "both") == 0) {
        double s = crankl_sheaf_resonance(sa.data(), sa.size(), sb.data(), sb.size());
        std::cout << "sheaf_resonance=" << s << "\n";
    }
    crankl_cran_close(&a);
    crankl_cran_close(&b);
    return 0;
}

static int cmd_turn(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr, *target_path = nullptr;
    int steps = 1;
    double lr = 0.01;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc)
            target_path = argv[++i];
        else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            steps = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--lr") == 0 && i + 1 < argc)
            lr = std::atof(argv[++i]);
    }
    if (!input || !output)
        return 1;
    crankl_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;

    std::vector<float> target;
    size_t target_count = 0;
    if (target_path)
        target = read_f32(target_path, target_count);

    std::vector<uint64_t> layer_stack;
    for (int s = 0; s < steps; ++s) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (target_path && !target.empty()) {
                size_t base = i * CRANKL_BLOCK_FLOATS;
                if (base < target.size())
                    crankl_turn_toward(&slots[i], lr, target.data() + base,
                                       std::min(target.size() - base, size_t(CRANKL_BLOCK_FLOATS)));
            } else {
                crankl_turn(&slots[i], lr);
            }
        }
        layer_stack.insert(layer_stack.end(), slots.begin(), slots.end());
    }
    crankl_cran_header_t hdr = cran.header;
    if (steps > 0)
        hdr.depth_max = static_cast<uint32_t>(steps);
    const uint64_t *stacks_ptr = layer_stack.empty() ? nullptr : layer_stack.data();
    crankl_cran_write(output, &hdr, slots.data(), stacks_ptr, nullptr);
    crankl_cran_close(&cran);
    return 0;
}

struct FinetuneHolonomyCtx {
    const float *calib_x;
    const float *calib_y;
    size_t dim;
};

static double finetune_holonomy_loss(const crankl_cran_t *cran, void *ctx) {
    auto *c = static_cast<FinetuneHolonomyCtx *>(ctx);
    return crankl_holonomy_mse(cran, c->calib_x, c->calib_y, c->dim);
}

static int cmd_finetune(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr, *target_path = nullptr;
    const char *calib_x_path = nullptr, *calib_y_path = nullptr;
    int steps = 200;
    double lr = 0.02;
    double recon_weight = 1.0;
    double task_weight = 0.1;
    bool json = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc)
            target_path = argv[++i];
        else if (std::strcmp(argv[i], "--calib-x") == 0 && i + 1 < argc)
            calib_x_path = argv[++i];
        else if (std::strcmp(argv[i], "--calib-y") == 0 && i + 1 < argc)
            calib_y_path = argv[++i];
        else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            steps = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--lr") == 0 && i + 1 < argc)
            lr = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--recon-weight") == 0 && i + 1 < argc)
            recon_weight = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--task-weight") == 0 && i + 1 < argc)
            task_weight = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--json") == 0)
            json = true;
    }
    if (!input || !output)
        return 1;

    crankl_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;

    std::vector<float> target;
    size_t target_count = 0;
    if (target_path)
        target = read_f32(target_path, target_count);

    std::vector<float> calib_x, calib_y;
    size_t calib_dim = 0;
    FinetuneHolonomyCtx holo_ctx{};
    crankl_loss_fn task_loss = nullptr;
    if (calib_x_path && calib_y_path) {
        calib_x = read_f32(calib_x_path, calib_dim);
        size_t y_count = 0;
        calib_y = read_f32(calib_y_path, y_count);
        if (calib_x.empty() || calib_y.empty())
            return 3;
        calib_dim = std::min(calib_x.size(), calib_y.size());
        holo_ctx.calib_x = calib_x.data();
        holo_ctx.calib_y = calib_y.data();
        holo_ctx.dim = calib_dim;
        task_loss = finetune_holonomy_loss;
    }

    crankl_cran_t view = cran;
    view.slots = slots.data();

    double recon_before = 0.0;
    if (!target.empty()) {
        for (size_t s = 0; s < slots.size(); ++s) {
            size_t base = s * CRANKL_BLOCK_FLOATS;
            if (base + CRANKL_BLOCK_FLOATS <= target.size())
                recon_before += crankl_decrank_frobenius_loss(slots[s], target.data() + base);
        }
    }
    double task_before =
        task_loss ? crankl_holonomy_mse(&view, holo_ctx.calib_x, holo_ctx.calib_y, holo_ctx.dim)
                  : 0.0;

    std::vector<uint64_t> layer_stack;
    for (int step = 0; step < steps; ++step) {
        if (crankl_finetune(slots.data(), slots.size(), &view,
                            target.empty() ? nullptr : target.data(), task_loss, &holo_ctx, 1, lr,
                            recon_weight, task_weight) != 0)
            return 4;
        layer_stack.insert(layer_stack.end(), slots.begin(), slots.end());
    }

    double recon_after = recon_before;
    if (!target.empty()) {
        recon_after = 0.0;
        for (size_t s = 0; s < slots.size(); ++s) {
            size_t base = s * CRANKL_BLOCK_FLOATS;
            if (base + CRANKL_BLOCK_FLOATS <= target.size())
                recon_after += crankl_decrank_frobenius_loss(slots[s], target.data() + base);
        }
    }
    double task_after =
        task_loss ? crankl_holonomy_mse(&view, holo_ctx.calib_x, holo_ctx.calib_y, holo_ctx.dim)
                  : 0.0;

    crankl_cran_header_t hdr = cran.header;
    hdr.depth_max = static_cast<uint32_t>(steps);
    const uint64_t *stacks_ptr = layer_stack.empty() ? nullptr : layer_stack.data();
    crankl_cran_write(output, &hdr, slots.data(), stacks_ptr, nullptr);

    if (json) {
        std::cout << "{\"recon_before\":" << recon_before << ",\"recon_after\":" << recon_after
                  << ",\"task_before\":" << task_before << ",\"task_after\":" << task_after
                  << ",\"steps\":" << steps << "}\n";
    } else {
        std::cout << "recon_before=" << recon_before << " recon_after=" << recon_after << "\n";
        if (task_loss)
            std::cout << "task_before=" << task_before << " task_after=" << task_after << "\n";
    }

    crankl_cran_close(&cran);
    return 0;
}

static int cmd_peel(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr;
    uint32_t layers = 1;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--layers") == 0 && i + 1 < argc)
            layers = static_cast<uint32_t>(std::atoi(argv[++i]));
    }
    if (!input || !output)
        return 1;
    crankl_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;
    uint32_t stack_depth = cran.header.depth_max;
    // The reader guarantees layers is non-null only for a validated stack
    // section, so a plain null test is the whole safety check.
    if (cran.layers)
        crankl_peel_stack(slots.data(), slots.size(), cran.layers, stack_depth, layers);
    else
        for (auto &w : slots)
            crankl_peel(&w, layers);
    crankl_cran_header_t hdr = cran.header;
    crankl_cran_write(output, &hdr, slots.data(), nullptr, nullptr);
    crankl_cran_close(&cran);
    return 0;
}

static int cmd_bind(int argc, char **argv) {
    if (argc < 5)
        return 1;
    const char *out = nullptr;
    for (int i = 4; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
    }
    if (!out)
        return 1;
    crankl_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;
    size_t n = std::min(sa.size(), sb.size());
    std::vector<uint64_t> merged(n);
    for (size_t i = 0; i < n; ++i)
        merged[i] = crankl_bind(sa[i], sb[i]);
    crankl_cran_header_t hdr = a.header;
    hdr.n_slots = n;
    crankl_cran_write(out, &hdr, merged.data(), nullptr, nullptr);
    crankl_cran_close(&a);
    crankl_cran_close(&b);
    return 0;
}

static int cmd_holonomy(int argc, char **argv) {
    const char *input = nullptr, *vec = nullptr, *output = nullptr;
    size_t batch = 1;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "--vector") == 0 && i + 1 < argc)
            vec = argv[++i];
        else if (std::strcmp(argv[i], "--batch") == 0 && i + 1 < argc)
            batch = static_cast<size_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
    }
    if (!input || !vec || !output || batch == 0)
        return 1;
    crankl_cran_t cran{};
    if (crankl_cran_read(input, &cran) != 0)
        return 2;
    size_t count = 0;
    auto x = read_f32(vec, count);
    if (count == 0 || count % batch != 0) {
        crankl_cran_close(&cran);
        std::fprintf(stderr, "holonomy: vector file must hold dim*batch floats\n");
        return 3;
    }
    size_t dim = count / batch;
    std::vector<float> y(count);
    int rc = batch > 1 ? crankl_holonomy_batch(&cran, x.data(), dim, batch, y.data())
                       : crankl_holonomy(&cran, x.data(), dim, y.data());
    crankl_cran_close(&cran);
    if (rc != 0)
        return 4;
    return write_f32(output, y.data(), y.size());
}

static int cmd_stats(int argc, char **argv) {
    if (argc < 3)
        return 1;
    crankl_cran_t cran{};
    if (crankl_cran_read(argv[2], &cran) != 0)
        return 2;
    std::cout << "n_slots=" << cran.header.n_slots << " depth_max=" << cran.header.depth_max
              << " gamma=" << cran.header.gamma << "\n";
    int b1 = crankl_sheaf_beta1_proxy(cran.slots, cran.header.n_slots);
    std::cout << "beta1_proxy=" << b1 << "\n";
    crankl_cran_close(&cran);
    return 0;
}

static int cmd_verify(int argc, char **argv) {
    if (argc < 3)
        return 1;
    crankl_cran_t cran{};
    if (crankl_cran_read(argv[2], &cran) != 0)
        return 2;
    int rc = crankl_cran_verify(&cran);
    crankl_cran_close(&cran);
    std::cout << (rc == 0 ? "ok\n" : "fail\n");
    return rc;
}

static int cmd_version(int, char **) {
    std::cout << "crankl " << CRANKL_VERSION_STRING;
    if (crankl_has_avx2())
        std::cout << " avx2=1";
    else
        std::cout << " avx2=0";
    std::cout << "\n";
    return 0;
}

static int cmd_diff(int argc, char **argv) {
    if (argc < 4)
        return 1;
    crankl_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;
    size_t n = std::min(sa.size(), sb.size());
    size_t changed = crankl_crank_diff_count(sa.data(), sb.data(), n);
    double ham = crankl_crank_diff_hamming(sa.data(), sb.data(), n);
    std::cout << "slots_changed=" << changed << "/" << n << " hamming=" << ham << "\n";
    crankl_cran_close(&a);
    crankl_cran_close(&b);
    return 0;
}

/* Flags may appear in any order; positionals are the non-flag arguments. */
static bool parse_json_flag(int argc, char **argv, int first, char *positionals[], int n_pos) {
    bool json = false;
    int found = 0;
    for (int i = first; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (argv[i][0] != '-' && found < n_pos) {
            positionals[found++] = argv[i];
        }
    }
    return json;
}

static int cmd_inspect(int argc, char **argv) {
    if (argc < 3)
        return 1;
    char *pos[1] = {nullptr};
    const bool json = parse_json_flag(argc, argv, 2, pos, 1);
    const char *path = pos[0];
    if (!path || !*path) {
        std::fprintf(stderr, "inspect: missing archive path\n");
        return 1;
    }

    crankl_cran_t cran{};
    if (crankl_cran_read(path, &cran) != 0) {
        std::fprintf(stderr, "inspect: cannot read archive '%s'\n", path);
        return 2;
    }

    crankl_archive_metrics_t metrics{};
    int rc = crankl_cran_compute_metrics(&cran, &metrics);
    crankl_cran_metadata_t meta{};
    int meta_rc = crankl_cran_read_metadata(&cran, &meta);

    if (rc != 0) {
        crankl_cran_close(&cran);
        return 3;
    }

    if (json) {
        std::cout << "{\"path\":\"" << path << "\"," << "\"mmap_size\":" << cran.mmap_size << ","
                  << "\"gamma\":" << cran.header.gamma << "," << "\"flags\":" << cran.header.flags
                  << "," << "\"has_metadata\":" << bool_text(meta_rc == 0) << ",";
        if (meta_rc == 0) {
            std::cout << "\"metadata\":{\"model\":\"" << meta.model_name << "\",\"hash\":\""
                      << meta.source_hash << "\"},";
        }
        int h0 = -1, h1 = -1;
        crankl_sheaf_cohomology(cran.slots, cran.header.n_slots, &h0, &h1);
        std::cout << "\"cohomology\":{\"h0\":" << h0 << ",\"h1\":" << h1 << "},";
        size_t tensor_count = 0;
        if (crankl_archive_tensor_count(path, &tensor_count) == 0 && tensor_count > 0) {
            std::vector<crankl_archive_tensor_t> tensors(tensor_count);
            if (crankl_archive_tensor_list(path, tensors.data(), tensors.size()) == 0) {
                std::cout << "\"tensors\":[";
                for (size_t t = 0; t < tensors.size(); ++t) {
                    if (t)
                        std::cout << ",";
                    std::cout << "{\"name\":\"" << tensors[t].name << "\","
                              << "\"slot_offset\":" << tensors[t].slot_offset << ","
                              << "\"n_slots\":" << tensors[t].n_slots << ","
                              << "\"n_floats\":" << tensors[t].n_floats << "," << "\"checksum\":\""
                              << tensors[t].checksum << "\"}";
                }
                std::cout << "],";
            }
        }
        std::cout << "\"metrics\":";
        print_metrics_json(metrics);
        std::cout << "}\n";
    } else {
        std::cout << "path=" << path << "\n"
                  << "mmap_size=" << cran.mmap_size << "\n"
                  << "gamma=" << cran.header.gamma << "\n"
                  << "flags=" << cran.header.flags << "\n"
                  << "has_metadata=" << bool_text(meta_rc == 0) << "\n";
        if (meta_rc == 0) {
            std::cout << "metadata_model=" << meta.model_name << "\n"
                      << "metadata_hash=" << meta.source_hash << "\n";
        }
        int h0 = -1, h1 = -1;
        crankl_sheaf_cohomology(cran.slots, cran.header.n_slots, &h0, &h1);
        std::cout << "cohomology_h0=" << h0 << "\n"
                  << "cohomology_h1=" << h1 << "\n";
        size_t tensor_count = 0;
        if (crankl_archive_tensor_count(path, &tensor_count) == 0 && tensor_count > 0) {
            std::vector<crankl_archive_tensor_t> tensors(tensor_count);
            if (crankl_archive_tensor_list(path, tensors.data(), tensors.size()) == 0) {
                std::cout << "tensors=" << tensor_count << "\n";
                for (size_t t = 0; t < tensors.size(); ++t)
                    std::cout << "tensor[" << t << "]=" << tensors[t].name
                              << " slot_offset=" << tensors[t].slot_offset
                              << " n_slots=" << tensors[t].n_slots
                              << " n_floats=" << tensors[t].n_floats << "\n";
            }
        }
        print_metrics_text(metrics);
    }

    crankl_cran_close(&cran);
    return 0;
}

static int cmd_compare(int argc, char **argv) {
    if (argc < 4)
        return 1;
    char *pos[2] = {nullptr, nullptr};
    const bool json = parse_json_flag(argc, argv, 2, pos, 2);
    if (!pos[0] || !pos[1]) {
        std::fprintf(stderr, "compare: expected two archive paths\n");
        return 1;
    }

    crankl_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(pos[0], sa, a) != 0) {
        std::fprintf(stderr, "compare: cannot read archive '%s'\n", pos[0]);
        return 2;
    }
    if (load_cran_slots(pos[1], sb, b) != 0) {
        std::fprintf(stderr, "compare: cannot read archive '%s'\n", pos[1]);
        return 2;
    }

    size_t n = std::min(sa.size(), sb.size());
    size_t changed = crankl_crank_diff_count(sa.data(), sb.data(), n);
    double ham = crankl_crank_diff_hamming(sa.data(), sb.data(), n);
    double sheaf = crankl_sheaf_resonance(sa.data(), sa.size(), sb.data(), sb.size());
    double h1 = crankl_sheaf_resonance_h1(sa.data(), sa.size(), sb.data(), sb.size());
    double clifford = 0.0;
    for (size_t i = 0; i < n; ++i)
        clifford += crankl_clifford_resonance(sa[i], sb[i]);
    if (n)
        clifford /= static_cast<double>(n);

    crankl_archive_metrics_t ma{}, mb{};
    crankl_compute_archive_metrics(sa.data(), sa.size(), &ma);
    crankl_compute_archive_metrics(sb.data(), sb.size(), &mb);

    if (json) {
        std::cout << "{\"a\":\"" << pos[0] << "\",\"b\":\"" << pos[1] << "\","
                  << "\"slots_changed\":" << changed << "," << "\"slots_compared\":" << n << ","
                  << "\"hamming\":" << ham << "," << "\"clifford_resonance\":" << clifford << ","
                  << "\"sheaf_resonance\":" << sheaf << "," << "\"sheaf_resonance_h1\":" << h1
                  << "," << "\"delta_trit_density\":" << (mb.trit_density - ma.trit_density) << ","
                  << "\"delta_energy\":" << (mb.clifford_energy - ma.clifford_energy) << "}\n";
    } else {
        std::cout << "slots_changed=" << changed << "/" << n << "\n"
                  << "hamming=" << ham << "\n"
                  << "clifford_resonance=" << clifford << "\n"
                  << "sheaf_resonance=" << sheaf << "\n"
                  << "sheaf_resonance_h1=" << h1 << "\n"
                  << "delta_trit_density=" << (mb.trit_density - ma.trit_density) << "\n"
                  << "delta_energy=" << (mb.clifford_energy - ma.clifford_energy) << "\n";
    }

    crankl_cran_close(&a);
    crankl_cran_close(&b);
    return 0;
}

static int cmd_pipeline(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr, *target_path = nullptr, *manifest = nullptr;
    const char *tensor = nullptr;
    int steps = 16;
    double lr = 0.02;
    size_t n_slots = 8;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc)
            target_path = argv[++i];
        else if (std::strcmp(argv[i], "--tensor") == 0 && i + 1 < argc)
            tensor = argv[++i];
        else if (std::strcmp(argv[i], "--manifest") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            steps = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--lr") == 0 && i + 1 < argc)
            lr = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--shape") == 0 && i + 1 < argc)
            n_slots = static_cast<size_t>(std::atoi(argv[++i]));
    }
    if (!input || !output)
        return 1;

    std::vector<float> data;
    size_t count = 0;
    if (tensor) {
        data = read_tensor_f32(input, tensor);
        if (data.empty())
            return 2;
        count = data.size();
    } else {
        data = read_f32(input, count);
        if (data.empty())
            return 2;
    }
    if (n_slots == 8 && count > 0)
        n_slots = crankl_pack_n_slots(count);

    std::vector<uint64_t> slots(n_slots);
    if (crankl_pack_f32(data.data(), count, slots.data(), n_slots, 0.1f, 0.01f) != 0)
        return 3;

    std::vector<float> target;
    size_t target_count = 0;
    if (target_path)
        target = read_f32(target_path, target_count);

    for (int s = 0; s < steps; ++s) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (!target.empty()) {
                size_t base = i * CRANKL_BLOCK_FLOATS;
                if (base < target.size())
                    crankl_turn_toward(&slots[i], lr, target.data() + base,
                                       std::min(target.size() - base, size_t(CRANKL_BLOCK_FLOATS)));
            } else {
                crankl_turn(&slots[i], lr);
            }
        }
    }

    crankl_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = static_cast<uint32_t>(std::max(1, steps));
    hdr.gamma = 1.0f;

    crankl_cran_metadata_t meta{};
    std::snprintf(meta.model_name, sizeof(meta.model_name), "crankl-flow");
    std::snprintf(meta.source_hash, sizeof(meta.source_hash), "steps=%d;lr=%.6g", steps, lr);

    if (crankl_cran_write_with_metadata(output, &hdr, slots.data(), &meta) != 0)
        return 4;

    crankl_archive_metrics_t metrics{};
    crankl_compute_archive_metrics(slots.data(), slots.size(), &metrics);
    if (write_manifest(manifest, input, output, steps, lr, metrics) != 0)
        return 5;

    std::cout << "pipeline_output=" << output << "\n";
    if (manifest)
        std::cout << "manifest=" << manifest << "\n";
    print_metrics_text(metrics);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    const char *cmd = argv[1];
    if (std::strcmp(cmd, "pack") == 0)
        return cmd_pack(argc, argv);
    if (std::strcmp(cmd, "unpack") == 0)
        return cmd_unpack(argc, argv);
    if (std::strcmp(cmd, "resonance") == 0)
        return cmd_resonance(argc, argv);
    if (std::strcmp(cmd, "turn") == 0)
        return cmd_turn(argc, argv);
    if (std::strcmp(cmd, "finetune") == 0)
        return cmd_finetune(argc, argv);
    if (std::strcmp(cmd, "peel") == 0)
        return cmd_peel(argc, argv);
    if (std::strcmp(cmd, "bind") == 0)
        return cmd_bind(argc, argv);
    if (std::strcmp(cmd, "holonomy") == 0)
        return cmd_holonomy(argc, argv);
    if (std::strcmp(cmd, "stats") == 0)
        return cmd_stats(argc, argv);
    if (std::strcmp(cmd, "verify") == 0)
        return cmd_verify(argc, argv);
    if (std::strcmp(cmd, "version") == 0)
        return cmd_version(argc, argv);
    if (std::strcmp(cmd, "diff") == 0)
        return cmd_diff(argc, argv);
    if (std::strcmp(cmd, "inspect") == 0)
        return cmd_inspect(argc, argv);
    if (std::strcmp(cmd, "compare") == 0)
        return cmd_compare(argc, argv);
    if (std::strcmp(cmd, "pipeline") == 0)
        return cmd_pipeline(argc, argv);
    usage();
    return 1;
}
