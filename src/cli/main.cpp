#include "crankle/crankle.h"
#include "crankle/version.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void usage() {
    std::cout << "crankle v" << CRANKLE_VERSION_STRING << " — Grand Unified Crank Theory engine\n"
              << "Usage: crankle <command> [options]\n"
              << "Commands: pack, unpack, resonance, turn, peel, bind, holonomy, stats, verify\n";
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

static int cmd_pack(int argc, char **argv) {
    const char *input = nullptr, *output = nullptr;
    size_t n_slots = 8;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
        else if (std::strcmp(argv[i], "--shape") == 0 && i + 1 < argc) {
            n_slots = static_cast<size_t>(std::atoi(argv[++i]));
        }
    }
    if (!input || !output)
        return 1;
    size_t count = 0;
    auto data = read_f32(input, count);
    if (data.empty())
        return 2;
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
    const char *input = nullptr, *output = nullptr;
    int steps = 1;
    double lr = 0.01;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc)
            input = argv[++i];
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output = argv[++i];
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
    for (int s = 0; s < steps; ++s) {
        for (auto &w : slots)
            crankle_turn(&w, lr);
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
    usage();
    return 1;
}
