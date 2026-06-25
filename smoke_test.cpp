#include "column.hpp"
#include "compression.hpp"
#include "filter.hpp"
#include "storage.hpp"
#include "query.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    // Schema
    Schema schema;
    schema.table_name = "t";
    schema.columns    = {{"v", DataType::INT32}};
    assert(schema.column_index("v") == 0);
    assert(schema.column_index("x") == -1);

    // Column chunk
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(4);
    chunk.values<int32_t>()[0] = 10;
    chunk.num_rows = 4;

    // Storage round-trip
    const std::string dir = "smoke_data";
    fs::remove_all(dir);
    StorageManager sm(dir);
    sm.create_table(schema);

    Column col;
    col.def = {"v", DataType::INT32};
    for (int c = 0; c < 4; ++c) {
        ColumnChunk ch;
        ch.type = DataType::INT32;
        ch.resize(50000);
        auto* d = ch.values<int32_t>();
        for (int i = 0; i < 50000; ++i) {
            d[i] = c * 50000 + i;
        }
        ch.num_rows = 50000;
        col.append_chunk(std::move(ch));
    }
    sm.write_column("t", col);
    Column loaded = sm.read_column("t", "v");
    assert(loaded.total_rows() == 200000u);

    // RLE compression
    std::vector<uint8_t> raw(65536 * 4);
    auto* vals = reinterpret_cast<int32_t*>(raw.data());
    for (size_t i = 0; i < 65536; ++i) {
        vals[i] = 42;
    }
    auto codec = make_codec(Codec::RLE);
    auto block = codec->compress(raw.data(), raw.size(), 4);
    assert(compression_ratio(raw.size(), block.data.size()) > 50.0);

    // Query
    Schema orders_schema;
    orders_schema.table_name = "orders";
    orders_schema.columns    = {
        {"amount", DataType::FLOAT64},
        {"status", DataType::INT32},
    };
    sm.create_table(orders_schema);

    Column amount_col, status_col;
    amount_col.def = {"amount", DataType::FLOAT64};
    status_col.def = {"status", DataType::INT32};

    ColumnChunk amount, status;
    amount.type = DataType::FLOAT64;
    amount.resize(3);
    amount.values<double>()[0] = 100;
    amount.values<double>()[1] = 200;
    amount.values<double>()[2] = 300;
    amount.num_rows = 3;

    status.type = DataType::INT32;
    status.resize(3);
    status.values<int32_t>()[0] = 1;
    status.values<int32_t>()[1] = 0;
    status.values<int32_t>()[2] = 1;
    status.num_rows = 3;

    amount_col.append_chunk(std::move(amount));
    status_col.append_chunk(std::move(status));
    sm.write_column("orders", amount_col);
    sm.write_column("orders", status_col);

    QueryExecutor exec(sm);
    QueryPlan plan;
    plan.table_name = "orders";
    plan.select_cols = {"amount"};
    Predicate pred;
    pred.col_name = "status";
    pred.op = CompareOp::EQ;
    pred.value.i32 = 1;
    plan.where = {pred};
    plan.aggregates = {{AggFunc::SUM, "amount", "total"}};

    auto result = exec.execute(plan);
    assert(result.agg_results.size() == 1);
    assert(result.agg_results[0].value == 400.0);

    fs::remove_all(dir);
    std::cout << "All smoke tests passed.\n";
    return 0;
}
