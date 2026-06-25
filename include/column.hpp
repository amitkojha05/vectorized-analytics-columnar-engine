#pragma once
#include "schema.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

enum class CompareOp;

static constexpr size_t CHUNK_SIZE = 65536;

struct ChunkStats {
    int64_t min         = 0;
    int64_t max         = 0;
    bool    initialized = false;
};

class AlignedBuffer {
public:
    AlignedBuffer() = default;
    AlignedBuffer(const AlignedBuffer& other) { copy_from(other); }
    AlignedBuffer& operator=(const AlignedBuffer& other) {
        if (this != &other) {
            copy_from(other);
        }
        return *this;
    }
    AlignedBuffer(AlignedBuffer&&) noexcept            = default;
    AlignedBuffer& operator=(AlignedBuffer&&) noexcept = default;

    void resize(size_t bytes, size_t alignment = 64);
    uint8_t*       data()       { return ptr_.get(); }
    const uint8_t* data() const { return ptr_.get(); }
    size_t         size() const { return size_; }

private:
    void copy_from(const AlignedBuffer& other) {
        if (other.size_ == 0 || !other.ptr_) {
            ptr_.reset();
            size_ = 0;
            return;
        }
        resize(other.size_, 64);
        std::memcpy(ptr_.get(), other.ptr_.get(), other.size_);
    }

    std::unique_ptr<uint8_t[], void(*)(void*)> ptr_{nullptr, nullptr};
    size_t size_ = 0;
};

struct ColumnChunk {
    DataType         type;
    size_t           num_rows  = 0;
    bool             has_nulls = false;
    ChunkStats       stats;
    AlignedBuffer    data;
    std::vector<uint8_t> null_bitmap;

    template<typename T> T* values() {
        return reinterpret_cast<T*>(data.data());
    }
    template<typename T> const T* values() const {
        return reinterpret_cast<const T*>(data.data());
    }

    void resize(size_t rows);
    bool is_null(size_t row) const;
    void set_null(size_t row, bool null);
    void finalize_stats();
    bool can_skip_gt(int64_t threshold) const;
    bool might_match_range(int64_t min_val, int64_t max_val) const;
    bool can_skip_predicate(CompareOp op, int64_t threshold) const;
};

struct Column {
    ColumnDef                def;
    std::vector<ColumnChunk> chunks;

    size_t total_rows() const;
    void   append_chunk(ColumnChunk chunk);
};
