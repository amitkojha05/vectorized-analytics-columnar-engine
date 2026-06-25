#include <benchmark/benchmark.h>
#include "compression.hpp"
#include <cstring>
#include <vector>

static std::vector<uint8_t> make_identical_int32(size_t n, int32_t v) {
    std::vector<uint8_t> raw(n * 4);
    auto* values = reinterpret_cast<int32_t*>(raw.data());
    for (size_t i = 0; i < n; ++i) {
        values[i] = v;
    }
    return raw;
}

static std::vector<uint8_t> make_sequential_int64(size_t n) {
    std::vector<uint8_t> raw(n * 8);
    auto* values = reinterpret_cast<int64_t*>(raw.data());
    for (size_t i = 0; i < n; ++i) {
        values[i] = static_cast<int64_t>(i);
    }
    return raw;
}

static void BM_RLE_Compress(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    auto raw = make_identical_int32(N, 7);
    auto codec = make_codec(Codec::RLE);

    for (auto _ : state) {
        auto block = codec->compress(raw.data(), raw.size(), 4);
        benchmark::DoNotOptimize(block.data.size());
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(raw.size()));
}
BENCHMARK(BM_RLE_Compress)->Arg(65536);

static void BM_Delta_Compress(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    auto raw = make_sequential_int64(N);
    auto codec = make_codec(Codec::DELTA);

    for (auto _ : state) {
        auto block = codec->compress(raw.data(), raw.size(), 8);
        benchmark::DoNotOptimize(block.data.size());
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(raw.size()));
}
BENCHMARK(BM_Delta_Compress)->Arg(65536);

static void BM_RLE_Ratio(benchmark::State& state) {
    auto raw = make_identical_int32(65536, 42);
    auto codec = make_codec(Codec::RLE);
    auto block = codec->compress(raw.data(), raw.size(), 4);
    benchmark::DoNotOptimize(compression_ratio(raw.size(), block.data.size()));
}
BENCHMARK(BM_RLE_Ratio);

BENCHMARK_MAIN();
