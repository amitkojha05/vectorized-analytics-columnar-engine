#include <benchmark/benchmark.h>
#include "scan.hpp"
#include "filter.hpp"
#include "column.hpp"

static Column make_int32_column(size_t n, const std::string& name) {
    Column col;
    col.def = {name, DataType::INT32};
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(n);
    auto* data = chunk.values<int32_t>();
    for (size_t i = 0; i < n; ++i) {
        data[i] = static_cast<int32_t>(i % 1000);
    }
    chunk.num_rows = n;
    col.chunks.push_back(std::move(chunk));
    return col;
}

static void BM_FullScan(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    auto col = make_int32_column(N, "value");

    for (auto _ : state) {
        SelectionVector sel;
        sel.fill_all(col.chunks[0].num_rows);
        int64_t sum = 0;
        auto* data = col.chunks[0].values<int32_t>();
        for (size_t i = 0; i < sel.count; ++i) {
            sum += data[sel.sel[i]];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(N) * sizeof(int32_t));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(N));
}
BENCHMARK(BM_FullScan)->RangeMultiplier(4)->Range(1 << 14, 1 << 22);

static void BM_FilteredScan_50pct(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    auto col = make_int32_column(N, "value");
    Predicate pred;
    pred.col_name = "value";
    pred.op = CompareOp::LT;
    pred.selectivity_hint = 0.5f;
    pred.value.i32 = 500;

    for (auto _ : state) {
        SelectionVector sel;
        evaluate_predicate(col.chunks[0], pred, sel);
        benchmark::DoNotOptimize(sel.count);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(N));
}
BENCHMARK(BM_FilteredScan_50pct)->RangeMultiplier(4)->Range(1 << 14, 1 << 22);

static void BM_RowAtATimeScan(benchmark::State& state) {
    const size_t N = static_cast<size_t>(state.range(0));
    auto col = make_int32_column(N, "value");

    for (auto _ : state) {
        int64_t sum = 0;
        auto* data = col.chunks[0].values<int32_t>();
        for (size_t i = 0; i < N; ++i) {
            if (data[i] < 500) {
                sum += data[i];
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(N) * sizeof(int32_t));
}
BENCHMARK(BM_RowAtATimeScan)->RangeMultiplier(4)->Range(1 << 14, 1 << 20);

BENCHMARK_MAIN();
