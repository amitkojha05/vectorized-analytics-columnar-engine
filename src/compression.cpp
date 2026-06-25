#include "compression.hpp"
#include <cstring>
#include <stdexcept>
#include <memory>
#include <vector>

#ifdef HAVE_LZ4
#include <lz4.h>
#endif

namespace {

// -------------------- ZigZag --------------------

uint64_t zigzag_encode(int64_t n) {
    return static_cast<uint64_t>((n << 1) ^ (n >> 63));
}

int64_t zigzag_decode(uint64_t n) {
    return static_cast<int64_t>((n >> 1) ^ (~(n & 1) + 1));
}

// -------------------- Varint helpers --------------------

void write_varint(std::vector<uint8_t>& out, uint64_t v) {
    while (v >= 0x80) {
        out.push_back(static_cast<uint8_t>(v | 0x80));
        v >>= 7;
    }
    out.push_back(static_cast<uint8_t>(v));
}

uint64_t read_varint(const uint8_t*& p) {
    uint64_t result = 0;
    int shift = 0;

    while (true) {
        uint8_t byte = *p++;
        result |= (uint64_t(byte & 0x7F) << shift);
        if (!(byte & 0x80)) break;
        shift += 7;
    }
    return result;
}

// -------------------- NONE --------------------

class NoneCodec : public ICodec {
public:
    CompressedBlock compress(const uint8_t* src, size_t len,
                             uint8_t element_size) override {
        CompressedBlock block;
        block.codec         = Codec::NONE;
        block.original_size = static_cast<uint32_t>(len);
        block.element_size  = element_size;
        block.data.assign(src, src + len);
        return block;
    }

    std::vector<uint8_t> decompress(const CompressedBlock& block) override {
        return block.data;
    }

    std::string name() const override { return "NONE"; }
};

// -------------------- RLE --------------------

class RLECodec : public ICodec {
public:
    CompressedBlock compress(const uint8_t* src, size_t len,
                             uint8_t element_size) override {
        if (len % element_size != 0) {
            throw std::invalid_argument("RLE: length not aligned");
        }

        CompressedBlock block;
        block.codec         = Codec::RLE;
        block.original_size = static_cast<uint32_t>(len);
        block.element_size  = element_size;

        const size_t count = len / element_size;
        size_t i = 0;

        while (i < count) {
            const uint8_t* value = src + i * element_size;
            uint32_t run = 1;

            while (i + run < count &&
                   std::memcmp(src + (i + run) * element_size,
                               value,
                               element_size) == 0) {
                ++run;
            }

            block.data.insert(block.data.end(), value, value + element_size);
            write_varint(block.data, run);

            i += run;
        }

        return block;
    }

    std::vector<uint8_t> decompress(const CompressedBlock& block) override {
        const size_t esize = block.element_size;
        std::vector<uint8_t> out(block.original_size);

        size_t out_pos = 0;
        size_t in_pos = 0;

        while (in_pos < block.data.size()) {
            const uint8_t* value = block.data.data() + in_pos;
            in_pos += esize;

            const uint8_t* p = block.data.data() + in_pos;
            uint64_t run = read_varint(p);
            in_pos = p - block.data.data();

            for (uint64_t r = 0; r < run; ++r) {
                std::memcpy(out.data() + out_pos, value, esize);
                out_pos += esize;
            }
        }

        return out;
    }

    std::string name() const override { return "RLE"; }
};

// -------------------- DELTA (FIXED) --------------------

class DeltaCodec : public ICodec {
public:
    CompressedBlock compress(const uint8_t* src, size_t len,
                             uint8_t element_size) override {

        if (element_size != 4 && element_size != 8) {
            throw std::invalid_argument("Delta supports 4 or 8 byte types");
        }

        CompressedBlock block;
        block.codec         = Codec::DELTA;
        block.original_size = static_cast<uint32_t>(len);
        block.element_size  = element_size;

        const size_t count = len / element_size;
        if (count == 0) return block;

        if (element_size == 4) {
            const int32_t* v = reinterpret_cast<const int32_t*>(src);

            int32_t prev = v[0];
            write_varint(block.data, zigzag_encode(prev));

            for (size_t i = 1; i < count; ++i) {
                int32_t delta = v[i] - prev;
                prev = v[i];
                write_varint(block.data, zigzag_encode(delta));
            }
        } else {
            const int64_t* v = reinterpret_cast<const int64_t*>(src);

            int64_t prev = v[0];
            write_varint(block.data, zigzag_encode(prev));

            for (size_t i = 1; i < count; ++i) {
                int64_t delta = v[i] - prev;
                prev = v[i];
                write_varint(block.data, zigzag_encode(delta));
            }
        }

        return block;
    }

    std::vector<uint8_t> decompress(const CompressedBlock& block) override {
        const size_t esize = block.element_size;
        const size_t count = block.original_size / esize;

        std::vector<uint8_t> out(block.original_size);
        if (count == 0) return out;

        const uint8_t* p = block.data.data();

        if (esize == 4) {
            auto* v = reinterpret_cast<int32_t*>(out.data());

            uint64_t first = read_varint(p);
            v[0] = static_cast<int32_t>(zigzag_decode(first));

            int32_t prev = v[0];

            for (size_t i = 1; i < count; ++i) {
                uint64_t zz = read_varint(p);
                int32_t delta = static_cast<int32_t>(zigzag_decode(zz));
                prev += delta;
                v[i] = prev;
            }
        } else {
            auto* v = reinterpret_cast<int64_t*>(out.data());

            uint64_t first = read_varint(p);
            v[0] = zigzag_decode(first);

            int64_t prev = v[0];

            for (size_t i = 1; i < count; ++i) {
                uint64_t zz = read_varint(p);
                int64_t delta = zigzag_decode(zz);
                prev += delta;
                v[i] = prev;
            }
        }

        return out;
    }

    std::string name() const override { return "DELTA"; }
};

// -------------------- LZ4 (optional) --------------------

#ifdef HAVE_LZ4
class LZ4CodecImpl : public ICodec {
public:
    CompressedBlock compress(const uint8_t* src, size_t len,
                             uint8_t element_size) override {
        CompressedBlock block;
        block.codec         = Codec::LZ4;
        block.original_size = static_cast<uint32_t>(len);
        block.element_size  = element_size;

        const int max_dst = LZ4_compressBound((int)len);
        block.data.resize(max_dst);

        int written = LZ4_compress_default(
            reinterpret_cast<const char*>(src),
            reinterpret_cast<char*>(block.data.data()),
            (int)len,
            max_dst);

        if (written <= 0) {
            throw std::runtime_error("LZ4 compression failed");
        }

        block.data.resize(written);
        return block;
    }

    std::vector<uint8_t> decompress(const CompressedBlock& block) override {
        std::vector<uint8_t> out(block.original_size);

        int decoded = LZ4_decompress_safe(
            reinterpret_cast<const char*>(block.data.data()),
            reinterpret_cast<char*>(out.data()),
            (int)block.data.size(),
            (int)out.size());

        if (decoded < 0) {
            throw std::runtime_error("LZ4 decompression failed");
        }

        return out;
    }

    std::string name() const override { return "LZ4"; }
};
#endif

} // namespace

// -------------------- Factory --------------------

std::unique_ptr<ICodec> make_codec(Codec c) {
    switch (c) {
        case Codec::NONE:
            return std::make_unique<NoneCodec>();
        case Codec::RLE:
            return std::make_unique<RLECodec>();
        case Codec::DELTA:
            return std::make_unique<DeltaCodec>();
#ifdef HAVE_LZ4
        case Codec::LZ4:
            return std::make_unique<LZ4CodecImpl>();
#endif
        default:
            throw std::invalid_argument("unknown codec");
    }
}

// -------------------- ratio --------------------

double compression_ratio(size_t original, size_t compressed) {
    if (compressed == 0) return 0.0;
    return double(original) / double(compressed);
}