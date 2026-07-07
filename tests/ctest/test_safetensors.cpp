#include "crankl_internal_api.hpp"

#include <cstdio>
#include <string>
#include <vector>

#ifndef GOLDEN_DIR
#define GOLDEN_DIR "tests/golden"
#endif

int main() {
    std::string path = std::string(GOLDEN_DIR) + "/tiny.safetensors";
    std::vector<float> out;
    if (crankl::io::read_safetensors_f32(path.c_str(), "weights", out) != 0) {
        std::fprintf(stderr, "FAIL: read safetensors %s\n", path.c_str());
        return 1;
    }
    if (out.size() != 4) {
        std::fprintf(stderr, "FAIL: expected 4 floats got %zu\n", out.size());
        return 1;
    }
    if (out[0] < 0.09f || out[0] > 0.11f) {
        std::fprintf(stderr, "FAIL: unexpected value %f\n", out[0]);
        return 1;
    }
    std::printf("test_safetensors ok n=%zu v0=%f\n", out.size(), out[0]);
    return 0;
}
