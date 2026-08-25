#include "crankl/crankl.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

static int write_f32(const std::string &path, const std::vector<float> &data) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return 1;
    std::fwrite(data.data(), sizeof(float), data.size(), f);
    std::fclose(f);
    return 0;
}

int main() {
    std::vector<float> data(32);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(i) * 0.05f - 0.5f;

    std::string in = std::filesystem::temp_directory_path().string() + "/" + "crankl_flow_in.f32";
    std::string out =
        std::filesystem::temp_directory_path().string() + "/" + "crankl_flow_out.crank";
    if (write_f32(in, data) != 0)
        return 1;

    std::vector<uint64_t> slots(4);
    if (crankl_pack_f32(data.data(), data.size(), slots.data(), slots.size(), 0.1f, 0.01f) !=
        CRANKL_OK)
        return 2;
    for (auto &slot : slots)
        crankl_turn(&slot, 0.03);

    crankl_cran_header_t hdr{};
    hdr.n_slots = slots.size();
    hdr.depth_max = 8;
    hdr.gamma = 1.0f;
    crankl_cran_metadata_t meta{};
    std::snprintf(meta.model_name, sizeof(meta.model_name), "flow-test");
    std::snprintf(meta.source_hash, sizeof(meta.source_hash), "unit");
    if (crankl_cran_write_with_metadata(out.c_str(), &hdr, slots.data(), &meta) != CRANKL_OK)
        return 3;

    crankl_cran_t cran{};
    if (crankl_cran_read(out.c_str(), &cran) != CRANKL_OK)
        return 4;
    crankl_archive_metrics_t metrics{};
    if (crankl_cran_compute_metrics(&cran, &metrics) != CRANKL_OK)
        return 5;
    crankl_cran_metadata_t loaded{};
    if (crankl_cran_read_metadata(&cran, &loaded) != CRANKL_OK)
        return 6;
    if (std::strcmp(loaded.model_name, "flow-test") != 0)
        return 7;
    crankl_cran_close(&cran);

    std::printf("test_flow_pipeline ok slots=%llu density=%f\n",
                static_cast<unsigned long long>(metrics.n_slots), metrics.trit_density);
    return 0;
}
