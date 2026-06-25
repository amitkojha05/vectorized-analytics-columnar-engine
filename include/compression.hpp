#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class Codec : uint8_t { NONE = 0, RLE = 1, DELTA = 2, LZ4 = 3 };

struct CompressedBlock {
    Codec                codec;
    uint32_t             original_size;
    uint8_t              element_size = 0;
    std::vector<uint8_t> data;
};

class ICodec {
public:
    virtual ~ICodec() = default;
    virtual CompressedBlock compress(const uint8_t* src, size_t len,
                                     uint8_t element_size) = 0;
    virtual std::vector<uint8_t> decompress(const CompressedBlock& block) = 0;
    virtual std::string name() const = 0;
};

std::unique_ptr<ICodec> make_codec(Codec c);

double compression_ratio(size_t original, size_t compressed);
