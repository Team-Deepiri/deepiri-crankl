#pragma once
#include <cstdint>
struct SheafCase {
    int n;
    uint64_t slots[6];
    int h0;
    int h1;
};
static const SheafCase SHEAF_CASES[] = {
    // two_aligned: h0=15, h1=0
    {2, {0x0010000000010000, 0x0010000000010000}, 15, 0},
    // two_orthogonal: h0=16, h1=0
    {2, {0x0010000000010000, 0x0010000000040000}, 16, 0},
    // chain3: h0=22, h1=0
    {3, {0x0010000000010000, 0x0010000000050000, 0x0010000000040000}, 22, 0},
    // triangle: h0=22, h1=1
    {3, {0x0010000000010000, 0x0010000000010000, 0x0010000000010000}, 22, 1},
    // bivec_pair: h0=15, h1=0
    {2, {0x0010000000400000, 0x0010000000400000}, 15, 0},
    // bivec_triangle: h0=22, h1=1
    {3, {0x0010000000400000, 0x0010000000400000, 0x0010000000400000}, 22, 1},
};
static const int SHEAF_CASE_COUNT = 6;
