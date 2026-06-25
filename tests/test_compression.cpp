#include "compression.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

TEST(CompressionTest, RLERoundTrip) {
    constexpr size_t n = 65536;
    std::vector<uint8_t> raw(n * 4);
    auto* values = reinterpret_cast<int32_t*>(raw.data());
    for (size_t i = 0; i < n; ++i) {
        values[i] = 42;
    }

    auto codec = make_codec(Codec::RLE);
    auto block = codec->compress(raw.data(), raw.size(), 4);
    auto out   = codec->decompress(block);

    EXPECT_EQ(out.size(), raw.size());
    EXPECT_EQ(std::memcmp(out.data(), raw.data(), raw.size()), 0);

    double ratio = compression_ratio(raw.size(), block.data.size());
    EXPECT_GT(ratio, 50.0);
}

TEST(CompressionTest, DeltaRoundTripInt64) {
    constexpr size_t n = 10000;
    std::vector<uint8_t> raw(n * 8);
    auto* values = reinterpret_cast<int64_t*>(raw.data());
    for (size_t i = 0; i < n; ++i) {
        values[i] = static_cast<int64_t>(1000 + i);
    }

    auto codec = make_codec(Codec::DELTA);
    auto block = codec->compress(raw.data(), raw.size(), 8);
    auto out   = codec->decompress(block);

    EXPECT_EQ(out.size(), raw.size());
    EXPECT_EQ(std::memcmp(out.data(), raw.data(), raw.size()), 0);

    double ratio = compression_ratio(raw.size(), block.data.size());
    EXPECT_GT(ratio, 4.0);
}

TEST(CompressionTest, DeltaRoundTripInt32) {
    std::vector<uint8_t> raw(4 * 5);
    auto* values = reinterpret_cast<int32_t*>(raw.data());
    values[0] = 100;
    values[1] = 105;
    values[2] = 103;
    values[3] = 110;
    values[4] = 108;

    auto codec = make_codec(Codec::DELTA);
    auto block = codec->compress(raw.data(), raw.size(), 4);
    auto out   = codec->decompress(block);

    EXPECT_EQ(std::memcmp(out.data(), raw.data(), raw.size()), 0);
}

TEST(CompressionTest, NoneRoundTrip) {
    std::vector<uint8_t> raw = {1, 2, 3, 4, 5, 6, 7, 8};
    auto codec = make_codec(Codec::NONE);
    auto block = codec->compress(raw.data(), raw.size(), 1);
    auto out   = codec->decompress(block);
    EXPECT_EQ(out, raw);
}

TEST(CompressionTest, RLEVariedRuns) {
    std::vector<uint8_t> raw(12);
    int32_t vals[] = {1, 1, 1, 2, 2, 3};
    std::memcpy(raw.data(), vals, sizeof(vals));

    auto codec = make_codec(Codec::RLE);
    auto block = codec->compress(raw.data(), 24, 4);
    auto out   = codec->decompress(block);
    EXPECT_EQ(std::memcmp(out.data(), vals, sizeof(vals)), 0);
}

#ifdef HAVE_LZ4
TEST(CompressionTest, LZ4RoundTrip) {
    std::vector<uint8_t> raw(4096);
    for (size_t i = 0; i < raw.size(); ++i) {
        raw[i] = static_cast<uint8_t>(i % 251);
    }
    auto codec = make_codec(Codec::LZ4);
    auto block = codec->compress(raw.data(), raw.size(), 1);
    auto out   = codec->decompress(block);
    EXPECT_EQ(out, raw);
}
#endif
