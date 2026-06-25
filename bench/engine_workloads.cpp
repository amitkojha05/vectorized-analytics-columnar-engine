#include "batch.hpp"
#include "column.hpp"
#include "engine_benchmark.hpp"
#include "filter.hpp"
#include "scan.hpp"
#include "storage.hpp"
#include <filesystem>
#include <iostream>
#include <system_error>
#include <vector>

static ColumnChunk make_int32_chunk(size_t n, int32_t modulo = 1000) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(n);
    auto* data = chunk.values<int32_t>();
    for (size_t i = 0; i < n; ++i) {
        data[i] = static_cast<int32_t>(i % modulo);
    }
    chunk.num_rows = n;
    chunk.finalize_stats();
    return chunk;
}

static void run_full_scan_benchmark(size_t n) {
    auto chunk = make_int32_chunk(n);
    const int32_t* data = chunk.values<int32_t>();

    auto result = run_benchmark(
        "full_scan",
        [&]() {
            int64_t sum = 0;
            for (size_t i = 0; i < n; i += BATCH_SIZE) {
                const size_t end       = std::min(i + BATCH_SIZE, n);
                const int32_t* batch   = data + i;
                const size_t batch_len = end - i;
                for (size_t j = 0; j < batch_len; ++j) {
                    sum += batch[j];
                }
            }
            (void)sum;
        },
        n);
    print_benchmark_result(result);
}

static void run_filtered_scan_benchmark(size_t n) {
    auto chunk = make_int32_chunk(n);
    Predicate pred;
    pred.col_name = "value";
    pred.op       = CompareOp::LT;
    pred.value.i32 = 500;

    auto result = run_benchmark(
        "filtered_scan",
        [&]() {
            SelectionVector sel;
            evaluate_predicate(chunk, pred, sel);
            (void)sel.count;
        },
        n);
    print_benchmark_result(result);
}

static void run_aggregation_benchmark(size_t n) {
    auto chunk = make_int32_chunk(n);
    SelectionVector sel;
    sel.fill_all(n);

    auto result = run_benchmark(
        "aggregation_sum",
        [&]() {
            const int32_t* data = chunk.values<int32_t>();
            int64_t sum         = 0;
            for (size_t i = 0; i < sel.count; i += BATCH_SIZE) {
                const size_t end = std::min(i + BATCH_SIZE, sel.count);
                for (size_t j = i; j < end; ++j) {
                    sum += data[sel.sel[j]];
                }
            }
            (void)sum;
        },
        n);
    print_benchmark_result(result);
}

static void run_compressed_scan_benchmark(size_t n) {
    const std::string tmp_dir = "bench_data_workloads";
    std::error_code ec;
    std::filesystem::remove_all(tmp_dir, ec);

    StorageManager sm(tmp_dir);
    Schema schema;
    schema.table_name = "bench";
    schema.columns    = {{"value", DataType::INT32}};
    sm.create_table(schema);

    Column col;
    col.def = {"value", DataType::INT32};
    col.append_chunk(make_int32_chunk(n));
    sm.write_column("bench", col, Codec::RLE);

    auto result = run_benchmark(
        "compressed_scan",
        [&]() {
            ScanConfig cfg;
            cfg.table_name = "bench";
            cfg.projection = {"value"};
            ScanOperator scan(sm, cfg);
            std::vector<ColumnChunk> batch;
            SelectionVector sel;
            int64_t sum = 0;
            while (scan.next_batch(batch, sel)) {
                const auto* data = batch[0].values<int32_t>();
                for (size_t i = 0; i < sel.count; ++i) {
                    sum += data[sel.sel[i]];
                }
            }
            (void)sum;
        },
        n);
    print_benchmark_result(result);

    std::filesystem::remove_all(tmp_dir, ec);
}

int main() {
    constexpr size_t N = CHUNK_SIZE;

    std::cout << "Columnar Engine Workload Benchmarks (N=" << N << ")\n";
    std::cout << "BATCH_SIZE=" << BATCH_SIZE << "\n\n";

    run_full_scan_benchmark(N);
    run_filtered_scan_benchmark(N);
    run_aggregation_benchmark(N);
    run_compressed_scan_benchmark(N);

    return 0;
}
