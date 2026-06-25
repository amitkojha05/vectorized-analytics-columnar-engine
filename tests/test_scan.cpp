#include "scan.hpp"
#include "storage.hpp"
#include "filter.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class ScanTest : public ::testing::Test {
protected:
    std::string tmp_dir = "test_data_scan";

    void SetUp() override { fs::remove_all(tmp_dir); }
    void TearDown() override { fs::remove_all(tmp_dir); }

    void create_table(size_t rows) {
        StorageManager sm(tmp_dir);
        Schema schema;
        schema.table_name = "data";
        schema.columns    = {{"value", DataType::INT32}};
        sm.create_table(schema);

        Column col;
        col.def = {"value", DataType::INT32};
        size_t written = 0;
        while (written < rows) {
            size_t n = std::min(CHUNK_SIZE, rows - written);
            ColumnChunk chunk;
            chunk.type = DataType::INT32;
            chunk.resize(n);
            auto* data = chunk.values<int32_t>();
            for (size_t i = 0; i < n; ++i) {
                data[i] = static_cast<int32_t>((written + i) % 1000);
            }
            chunk.num_rows = n;
            chunk.finalize_stats();
            col.append_chunk(std::move(chunk));
            written += n;
        }
        sm.write_column("data", col);
    }
};

TEST_F(ScanTest, FullScanNoFilter) {
    create_table(100000);

    StorageManager sm(tmp_dir);
    ScanConfig cfg;
    cfg.table_name = "data";
    cfg.projection = {"value"};

    ScanOperator scan(sm, cfg);
    std::vector<ColumnChunk> batch;
    SelectionVector sel;

    int64_t sum = 0;
    size_t rows = 0;
    while (scan.next_batch(batch, sel)) {
        rows += sel.count;
        const auto* data = batch[0].values<int32_t>();
        for (size_t i = 0; i < sel.count; ++i) {
            sum += data[sel.sel[i]];
        }
    }

    EXPECT_EQ(rows, 100000u);
    int64_t expected = 0;
    for (size_t i = 0; i < 100000; ++i) {
        expected += static_cast<int64_t>(i % 1000);
    }
    EXPECT_EQ(sum, expected);
}

TEST_F(ScanTest, FilteredScan) {
    create_table(50000);

    StorageManager sm(tmp_dir);
    ScanConfig cfg;
    cfg.table_name = "data";
    cfg.projection = {"value"};
    Predicate pred;
    pred.col_name = "value";
    pred.op = CompareOp::LT;
    pred.selectivity_hint = 0.5f;
    pred.value.i32 = 500;
    cfg.filters.push_back(pred);

    ScanOperator scan(sm, cfg);
    std::vector<ColumnChunk> batch;
    SelectionVector sel;

    size_t count = 0;
    while (scan.next_batch(batch, sel)) {
        const auto* data = batch[0].values<int32_t>();
        for (size_t i = 0; i < sel.count; ++i) {
            EXPECT_LT(data[sel.sel[i]], 500);
            ++count;
        }
    }
    EXPECT_EQ(count, 25000u);
}

TEST(ScanGtInt32Test, BranchlessPattern) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(8);
    for (int i = 0; i < 8; ++i) {
        chunk.values<int32_t>()[i] = i;
    }
    chunk.num_rows = 8;

    SelectionVector sel;
    scan_gt_int32(chunk.values<int32_t>(), 8, 3, sel);
    EXPECT_EQ(sel.count, 4u);
}
