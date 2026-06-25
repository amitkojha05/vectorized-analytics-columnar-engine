#include "batch.hpp"
#include <algorithm>

std::vector<uint32_t> filter_gt_int32_batch(const int32_t* input,
                                            size_t n,
                                            int32_t threshold) {
    std::vector<uint32_t> result;
    result.reserve(n / 4);

    for (size_t i = 0; i < n; i += BATCH_SIZE) {
        const size_t end       = std::min(i + BATCH_SIZE, n);
        const int32_t* batch   = input + i;
        const size_t batch_len = end - i;

        for (size_t j = 0; j < batch_len; ++j) {
            if (batch[j] > threshold) {
                result.push_back(static_cast<uint32_t>(i + j));
            }
        }
    }
    return result;
}
