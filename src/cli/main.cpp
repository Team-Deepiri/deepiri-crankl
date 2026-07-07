#include "crankle/crankle.h"
#include "crankle/version.h"
#include "crankle_internal_api.hpp"

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
    std::cout << "crankle v" << CRANKLE_VERSION_STRING << " — Grand Unified Crank Theory engine\n"
              << "Usage: crankle <command> [options]\n"
              << "Commands: pack, unpack, resonance, turn, peel, bind, holonomy, stats, verify, diff,\n"
              << "          inspect, compare, pipeline, version\n";
}

static std::vector<float> read_f32(const char *path, size_t &count) {
    FILE *f = std::fopen(path, "rb");
    if (!f)
        return {};
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    count = static_cast<size_t>(sz / sizeof(float));
    std::vector<float> data(count);
    std::fread(data.data(), sizeof(float), count, f);
    std::fclose(f);
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

static int load_cran_slots(const char *path, std::vector<uint64_t> &slots, crankle_cran_t &cran) {
    if (crankle_cran_read(path, &cran) != 0)
        return -1;
    slots.assign(cran.slots, cran.slots + cran.header.n_slots);
    return 0;
}

static const char *bool_text(bool v) { return v ? "true" : "false"; }

static void print_metrics_text(const crankle_archive_metrics_t &m) {
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

static void print_metrics_json(const crankle_archive_metrics_t &m) {
    std::cout << "{"
              << "\"n_slots\":" << m.n_slots << ","
              << "\"depth_min\":" << m.depth_min << ","
              << "\"depth_max\":" << m.depth_max << ","
              << "\"scalar_mean\":" << m.scalar_mean << ","
              << "\"scalar_abs_mean\":" << m.scalar_abs_mean << ","
              << "\"trit_density\":" << m.trit_density << ","
              << "\"trit_entropy\":" << m.trit_entropy << ","
              << "\"clifford_energy\":" << m.clifford_energy << ","
              << "\"beta1_proxy\":" << m.beta1_proxy << "}";
}

static int write_manifest(const char *path, const char *input, const char *output, int steps,
                          double lr, const crankle_archive_metrics_t &metrics) {
    if (!path)
        return 0;
    FILE *f = std::fopen(path, "wb");
    if (!f)
        return 1;
    std::time_t now = std::time(nullptr);
    std::fprintf(f,
                 "{\n"
                 "  \"tool\": \"crankle\",\n"
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
                 CRANKLE_VERSION_STRING, static_cast<long long>(now), input ? input : "",
                 output ? output : "", steps, lr, static_cast<unsigned long long>(metrics.n_slots),
                 metrics.trit_density, metrics.trit_entropy, metrics.clifford_energy,
                 metrics.beta1_proxy);
    std::fclose(f);
    return 0;
}

static int cmd_pack(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr, *tensor = nullptr;
    size_t n_slots = 8;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--tensor") == 0 && i + 1 < argc)
            tensor = argv[++i];
        else if (std::strcmp(argv[i], "--shape") == 0 && i + 1 < argc) {
            n_slots = static_cast<size_t>(std::atoi(argv[++i]));
        }
    }
    if (!input || !output)
        return 1;

    std::vector<float> data;
    size_t count = 0;

    if (tensor) {
        if (crankle::io::read_safetensors_f32(input, tensor, data) != 0)
            return 2;
        count = data.size();
    } else {
        data = read_f32(input, count);
        if (data.empty())
            return 2;
    }

    if (n_slots == 8 && count > 0)
        n_slots = (count + 7) / 8;

    std::vector<uint64_t> slots(n_slots);
    crankle_pack_f32(data.data(), count, slots.data(), n_slots, 0.1f, 0.01f);
    crankle_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;
    return crankle_cran_write(output, &hdr, slots.data(), nullptr, nullptr);
}

static int cmd_unpack(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
    }
    if (!input || !output)
        return 1;
    crankle_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;
    std::vector<float> out(slots.size() * 8);
    crankle_unpack_f32(slots.data(), slots.size(), out.data(), out.size());
    crankle_cran_close(&cran);
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
    crankle_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;
    if (std::strcmp(mode, "clifford") == 0 || std::strcmp(mode, "both") == 0) {
        double r = 0;
        size_t n = std::min(sa.size(), sb.size());
        for (size_t i = 0; i < n; ++i)
            r += crankle_clifford_resonance(sa[i], sb[i]);
        if (n)
            r /= static_cast<double>(n);
        std::cout << "clifford_resonance=" << r << "\n";
    }
    if (std::strcmp(mode, "sheaf") == 0 || std::strcmp(mode, "both") == 0) {
        double s = crankle_sheaf_resonance(sa.data(), sa.size(), sb.data(), sb.size());
        std::cout << "sheaf_resonance=" << s << "\n";
    }
    crankle_cran_close(&a);
    crankle_cran_close(&b);
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
    crankle_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;

    std::vector<float> target;
    size_t target_count = 0;
    if (target_path)
        target = read_f32(target_path, target_count);

    for (int s = 0; s < steps; ++s) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (target_path && !target.empty()) {
                size_t base = i * 8;
                if (base < target.size())
                    crankle_turn_toward(&slots[i], lr, target.data() + base,
                                        std::min(target.size() - base, size_t(8)));
            } else {
                crankle_turn(&slots[i], lr);
            }
        }
    }
    crankle_cran_header_t hdr = cran.header;
    crankle_cran_write(output, &hdr, slots.data(), nullptr, nullptr);
    crankle_cran_close(&cran);
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
    crankle_cran_t cran{};
    std::vector<uint64_t> slots;
    if (load_cran_slots(input, slots, cran) != 0)
        return 2;
    for (auto &w : slots)
        crankle_peel(&w, layers);
    crankle_cran_header_t hdr = cran.header;
    crankle_cran_write(output, &hdr, slots.data(), nullptr, nullptr);
    crankle_cran_close(&cran);
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
    crankle_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;
    size_t n = std::min(sa.size(), sb.size());
    std::vector<uint64_t> merged(n);
    for (size_t i = 0; i < n; ++i)
        merged[i] = crankle_bind(sa[i], sb[i]);
    crankle_cran_header_t hdr = a.header;
    hdr.n_slots = n;
    crankle_cran_write(out, &hdr, merged.data(), nullptr, nullptr);
    crankle_cran_close(&a);
    crankle_cran_close(&b);
    return 0;
}

static int cmd_holonomy(int argc, char **argv) {
    const char *input = nullptr, *vec = nullptr, *output = nullptr;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "--vector") == 0 && i + 1 < argc)
            vec = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
    }
    if (!input || !vec || !output)
        return 1;
    crankle_cran_t cran{};
    if (crankle_cran_read(input, &cran) != 0)
        return 2;
    size_t count = 0;
    auto x = read_f32(vec, count);
    std::vector<float> y(count);
    crankle_holonomy(&cran, x.data(), count, y.data());
    crankle_cran_close(&cran);
    return write_f32(output, y.data(), y.size());
}

static int cmd_stats(int argc, char **argv) {
    if (argc < 3)
        return 1;
    crankle_cran_t cran{};
    if (crankle_cran_read(argv[2], &cran) != 0)
        return 2;
    std::cout << "n_slots=" << cran.header.n_slots << " depth_max=" << cran.header.depth_max
              << " gamma=" << cran.header.gamma << "\n";
    int b1 = crankle_sheaf_beta1_proxy(cran.slots, cran.header.n_slots);
    std::cout << "beta1_proxy=" << b1 << "\n";
    crankle_cran_close(&cran);
    return 0;
}

static int cmd_verify(int argc, char **argv) {
    if (argc < 3)
        return 1;
    crankle_cran_t cran{};
    if (crankle_cran_read(argv[2], &cran) != 0)
        return 2;
    int rc = crankle_cran_verify(&cran);
    crankle_cran_close(&cran);
    std::cout << (rc == 0 ? "ok\n" : "fail\n");
    return rc;
}

static int cmd_version(int, char **) {
    std::cout << "crankle " << CRANKLE_VERSION_STRING;
    if (crankle_has_avx2())
        std::cout << " avx2=1";
    else
        std::cout << " avx2=0";
    std::cout << "\n";
    return 0;
}

static int cmd_diff(int argc, char **argv) {
    if (argc < 4)
        return 1;
    crankle_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;
    size_t n = std::min(sa.size(), sb.size());
    size_t changed = crankle_crank_diff_count(sa.data(), sb.data(), n);
    double ham = crankle_crank_diff_hamming(sa.data(), sb.data(), n);
    std::cout << "slots_changed=" << changed << "/" << n << " hamming=" << ham << "\n";
    crankle_cran_close(&a);
    crankle_cran_close(&b);
    return 0;
}

static int cmd_inspect(int argc, char **argv) {
    if (argc < 3)
        return 1;
    bool json = false;
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0)
            json = true;
    }

    crankle_cran_t cran{};
    if (crankle_cran_read(argv[2], &cran) != 0)
        return 2;

    crankle_archive_metrics_t metrics{};
    int rc = crankle_cran_compute_metrics(&cran, &metrics);
    crankle_cran_metadata_t meta{};
    int meta_rc = crankle_cran_read_metadata(&cran, &meta);

    if (rc != 0) {
        crankle_cran_close(&cran);
        return 3;
    }

    if (json) {
        std::cout << "{\"path\":\"" << argv[2] << "\","
                  << "\"mmap_size\":" << cran.mmap_size << ","
                  << "\"gamma\":" << cran.header.gamma << ","
                  << "\"flags\":" << cran.header.flags << ","
                  << "\"has_metadata\":" << bool_text(meta_rc == 0) << ",";
        if (meta_rc == 0) {
            std::cout << "\"metadata\":{\"model\":\"" << meta.model_name << "\",\"hash\":\""
                      << meta.source_hash << "\"},";
        }
        std::cout << "\"metrics\":";
        print_metrics_json(metrics);
        std::cout << "}\n";
    } else {
        std::cout << "path=" << argv[2] << "\n"
                  << "mmap_size=" << cran.mmap_size << "\n"
                  << "gamma=" << cran.header.gamma << "\n"
                  << "flags=" << cran.header.flags << "\n"
                  << "has_metadata=" << bool_text(meta_rc == 0) << "\n";
        if (meta_rc == 0) {
            std::cout << "metadata_model=" << meta.model_name << "\n"
                      << "metadata_hash=" << meta.source_hash << "\n";
        }
        print_metrics_text(metrics);
    }

    crankle_cran_close(&cran);
    return 0;
}

static int cmd_compare(int argc, char **argv) {
    if (argc < 4)
        return 1;
    bool json = false;
    for (int i = 4; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0)
            json = true;
    }

    crankle_cran_t a{}, b{};
    std::vector<uint64_t> sa, sb;
    if (load_cran_slots(argv[2], sa, a) != 0 || load_cran_slots(argv[3], sb, b) != 0)
        return 2;

    size_t n = std::min(sa.size(), sb.size());
    size_t changed = crankle_crank_diff_count(sa.data(), sb.data(), n);
    double ham = crankle_crank_diff_hamming(sa.data(), sb.data(), n);
    double sheaf = crankle_sheaf_resonance(sa.data(), sa.size(), sb.data(), sb.size());
    double clifford = 0.0;
    for (size_t i = 0; i < n; ++i)
        clifford += crankle_clifford_resonance(sa[i], sb[i]);
    if (n)
        clifford /= static_cast<double>(n);

    crankle_archive_metrics_t ma{}, mb{};
    crankle_compute_archive_metrics(sa.data(), sa.size(), &ma);
    crankle_compute_archive_metrics(sb.data(), sb.size(), &mb);

    if (json) {
        std::cout << "{\"a\":\"" << argv[2] << "\",\"b\":\"" << argv[3] << "\","
                  << "\"slots_changed\":" << changed << ","
                  << "\"slots_compared\":" << n << ","
                  << "\"hamming\":" << ham << ","
                  << "\"clifford_resonance\":" << clifford << ","
                  << "\"sheaf_resonance\":" << sheaf << ","
                  << "\"delta_trit_density\":" << (mb.trit_density - ma.trit_density) << ","
                  << "\"delta_energy\":" << (mb.clifford_energy - ma.clifford_energy) << "}\n";
    } else {
        std::cout << "slots_changed=" << changed << "/" << n << "\n"
                  << "hamming=" << ham << "\n"
                  << "clifford_resonance=" << clifford << "\n"
                  << "sheaf_resonance=" << sheaf << "\n"
                  << "delta_trit_density=" << (mb.trit_density - ma.trit_density) << "\n"
                  << "delta_energy=" << (mb.clifford_energy - ma.clifford_energy) << "\n";
    }

    crankle_cran_close(&a);
    crankle_cran_close(&b);
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
        if (crankle::io::read_safetensors_f32(input, tensor, data) != 0)
            return 2;
        count = data.size();
    } else {
        data = read_f32(input, count);
        if (data.empty())
            return 2;
    }
    if (n_slots == 8 && count > 0)
        n_slots = (count + 7) / 8;

    std::vector<uint64_t> slots(n_slots);
    if (crankle_pack_f32(data.data(), count, slots.data(), n_slots, 0.1f, 0.01f) != 0)
        return 3;

    std::vector<float> target;
    size_t target_count = 0;
    if (target_path)
        target = read_f32(target_path, target_count);

    for (int s = 0; s < steps; ++s) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (!target.empty()) {
                size_t base = i * 8;
                if (base < target.size())
                    crankle_turn_toward(&slots[i], lr, target.data() + base,
                                        std::min(target.size() - base, size_t(8)));
            } else {
                crankle_turn(&slots[i], lr);
            }
        }
    }

    crankle_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = static_cast<uint32_t>(std::max(1, steps));
    hdr.gamma = 1.0f;

    crankle_cran_metadata_t meta{};
    std::snprintf(meta.model_name, sizeof(meta.model_name), "crankle-flow");
    std::snprintf(meta.source_hash, sizeof(meta.source_hash), "steps=%d;lr=%.6g", steps, lr);

    if (crankle_cran_write_with_metadata(output, &hdr, slots.data(), &meta) != 0)
        return 4;

    crankle_archive_metrics_t metrics{};
    crankle_compute_archive_metrics(slots.data(), slots.size(), &metrics);
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
