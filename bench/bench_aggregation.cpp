#include <benchmark/benchmark.h>
#include "aggregator.hpp"
#include "column.hpp"

static Column make_int32_column(size_t n) {
    Column col;
    col.def = {"value", DataType::INT32};

    size_t written = 0;
    while (written < n) {
        const size_t chunk_n = std::min(CHUNK_SIZE, n - written);
        ColumnChunk chunk;
        chunk.type = DataType::INT32;
        chunk.resize(chunk_n);
        auto* data = chunk.values<int32_t>();
        for (size_t i = 0; i < chunk_n; ++i) {
            data[i] = static_cast<int32_t>((written + i) % 1000);
        }
        chunk.num_rows = chunk_n;
        col.append_chunk(std::move(chunk));
        written += chunk_n;
    }
    return col;
}

static void BM_AggregationSum(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    auto col = make_int32_column(N);

    for (auto _ : state) {
        Aggregator agg({{AggFunc::SUM, "value", "total"}});
        for (const auto& chunk : col.chunks) {
            SelectionVector sel;
            sel.fill_all(chunk.num_rows);
            agg.accumulate({chunk}, {"value"}, sel);
        }
        auto result = agg.finalize();
        benchmark::DoNotOptimize(result[0].value);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(N));
}
BENCHMARK(BM_AggregationSum)->Arg(1 << 20)->Arg(1 << 22);

static void BM_RowMajorSum(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    std::vector<int32_t> data(N);
    for (size_t i = 0; i < N; ++i) {
        data[i] = static_cast<int32_t>(i % 1000);
    }

    for (auto _ : state) {
        int64_t sum = 0;
        for (size_t i = 0; i < N; ++i) {
            sum += data[i];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(N));
}
BENCHMARK(BM_RowMajorSum)->Arg(1 << 20);

BENCHMARK_MAIN();
