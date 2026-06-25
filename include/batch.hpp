#pragma once
#include <cstdint>
#include <vector>

static constexpr size_t BATCH_SIZE = 1024;

struct Batch {
    const uint8_t* data;
    const uint8_t* nulls;
    size_t         size;
};

std::vector<uint32_t> filter_gt_int32_batch(const int32_t* input,
                                            size_t n,
                                            int32_t threshold);
