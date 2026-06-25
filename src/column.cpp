#include "column.hpp"
#include "filter.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {

void free_aligned(void* p) {
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

void* alloc_aligned(size_t alignment, size_t size) {
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

}  // namespace

void AlignedBuffer::resize(size_t bytes, size_t alignment) {
    size_t alloc = (bytes + alignment - 1) & ~(alignment - 1);
    if (alloc == size_ && ptr_) {
        return;
    }
    uint8_t* raw = static_cast<uint8_t*>(alloc_aligned(alignment, alloc));
    ptr_ = std::unique_ptr<uint8_t[], void(*)(void*)>(raw, free_aligned);
    size_ = alloc;
}

void ColumnChunk::resize(size_t rows) {
    const size_t esize = element_size(type);
    data.resize(rows * esize, 64);
    null_bitmap.assign((rows + 7) / 8, 0xFF);
    has_nulls = false;
    num_rows = rows;
}

bool ColumnChunk::is_null(size_t row) const {
    if (!has_nulls) {
        return false;
    }
    assert(row < num_rows);
    const size_t byte = row / 8;
    const size_t bit  = row % 8;
    return (null_bitmap[byte] & (1u << bit)) == 0;
}

void ColumnChunk::set_null(size_t row, bool null) {
    assert(row < num_rows);
    has_nulls = has_nulls || null;
    const size_t byte = row / 8;
    const size_t bit  = row % 8;
    if (null) {
        null_bitmap[byte] &= static_cast<uint8_t>(~(1u << bit));
    } else {
        null_bitmap[byte] |= static_cast<uint8_t>(1u << bit);
    }
}

namespace {

template<typename T>
void compute_typed_stats(const ColumnChunk& chunk, int64_t& min_v, int64_t& max_v,
                         bool& found) {
    const T* data = chunk.values<T>();
    for (size_t i = 0; i < chunk.num_rows; ++i) {
        if (chunk.has_nulls && chunk.is_null(i)) {
            continue;
        }
        const int64_t v = static_cast<int64_t>(data[i]);
        if (!found) {
            min_v = v;
            max_v = v;
            found = true;
        } else {
            min_v = std::min(min_v, v);
            max_v = std::max(max_v, v);
        }
    }
}

}  // namespace

void ColumnChunk::finalize_stats() {
    if (num_rows == 0) {
        stats.initialized = false;
        return;
    }

    int64_t min_v = 0;
    int64_t max_v = 0;
    bool found    = false;

    switch (type) {
        case DataType::INT32:
            compute_typed_stats<int32_t>(*this, min_v, max_v, found);
            break;
        case DataType::INT64:
            compute_typed_stats<int64_t>(*this, min_v, max_v, found);
            break;
        case DataType::FLOAT32:
            compute_typed_stats<float>(*this, min_v, max_v, found);
            break;
        case DataType::FLOAT64:
            compute_typed_stats<double>(*this, min_v, max_v, found);
            break;
        default:
            stats.initialized = false;
            return;
    }

    if (found) {
        stats.min         = min_v;
        stats.max         = max_v;
        stats.initialized = true;
    } else {
        stats.initialized = false;
    }
}

bool ColumnChunk::can_skip_gt(int64_t threshold) const {
    return stats.initialized && stats.max <= threshold;
}

bool ColumnChunk::might_match_range(int64_t min_val, int64_t max_val) const {
    if (!stats.initialized) {
        return true;
    }
    return !(stats.max < min_val || stats.min > max_val);
}

bool ColumnChunk::can_skip_predicate(CompareOp op, int64_t threshold) const {
    if (!stats.initialized) {
        return false;
    }
    switch (op) {
        case CompareOp::GT:
            return stats.max <= threshold;
        case CompareOp::GTE:
            return stats.max < threshold;
        case CompareOp::LT:
            return stats.min >= threshold;
        case CompareOp::LTE:
            return stats.min > threshold;
        case CompareOp::EQ:
            return stats.min > threshold || stats.max < threshold;
        case CompareOp::NEQ:
            return stats.min == stats.max && stats.min == threshold;
    }
    return false;
}

size_t Column::total_rows() const {
    size_t total = 0;
    for (const auto& chunk : chunks) {
        total += chunk.num_rows;
    }
    return total;
}

void Column::append_chunk(ColumnChunk chunk) {
    chunks.push_back(std::move(chunk));
}
