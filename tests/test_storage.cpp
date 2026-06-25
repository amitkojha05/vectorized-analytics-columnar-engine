#include "storage.hpp"
#include "column.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class StorageTest : public ::testing::Test {
protected:
    std::string tmp_dir = "test_data_storage";

    void SetUp() override {
        fs::remove_all(tmp_dir);
    }

    void TearDown() override {
        fs::remove_all(tmp_dir);
    }
};

TEST_F(StorageTest, WriteRead200KInt32) {
    StorageManager sm(tmp_dir);

    Schema schema;
    schema.table_name = "test_table";
    schema.columns    = {{"value", DataType::INT32}};
    sm.create_table(schema);

    Column col;
    col.def = {"value", DataType::INT32};

    constexpr size_t total = 200000;
    size_t written = 0;
    while (written < total) {
        const size_t n = std::min(CHUNK_SIZE, total - written);
        ColumnChunk chunk;
        chunk.type = DataType::INT32;
        chunk.resize(n);
        auto* data = chunk.values<int32_t>();
        for (size_t i = 0; i < n; ++i) {
            data[i] = static_cast<int32_t>(written + i);
        }
        chunk.num_rows = n;
        col.append_chunk(std::move(chunk));
        written += n;
    }

    sm.write_column("test_table", col);
    Column loaded = sm.read_column("test_table", "value");

    ASSERT_EQ(loaded.total_rows(), total);
    size_t idx = 0;
    for (const auto& chunk : loaded.chunks) {
        const auto* data = chunk.values<int32_t>();
        for (size_t i = 0; i < chunk.num_rows; ++i) {
            ASSERT_EQ(data[i], static_cast<int32_t>(idx));
            ++idx;
        }
    }
}

TEST_F(StorageTest, SchemaJsonValid) {
    StorageManager sm(tmp_dir);

    Schema schema;
    schema.table_name = "orders";
    schema.columns    = {
        {"order_id", DataType::INT64},
        {"amount", DataType::FLOAT64},
    };
    sm.create_table(schema);

    const std::string json =
        fs::path(tmp_dir + "/orders/schema.json").string();
    ASSERT_TRUE(fs::exists(json));

    Schema loaded = sm.load_schema("orders");
    EXPECT_EQ(loaded.table_name, "orders");
    EXPECT_EQ(loaded.columns.size(), 2u);
    EXPECT_EQ(loaded.column_index("order_id"), 0);
}

TEST_F(StorageTest, StreamingReader) {
    StorageManager sm(tmp_dir);

    Schema schema;
    schema.table_name = "stream";
    schema.columns    = {{"x", DataType::INT32}};
    sm.create_table(schema);

    Column col;
    col.def = {"x", DataType::INT32};
    for (int c = 0; c < 3; ++c) {
        ColumnChunk chunk;
        chunk.type = DataType::INT32;
        chunk.resize(10);
        auto* data = chunk.values<int32_t>();
        for (int i = 0; i < 10; ++i) {
            data[i] = c * 10 + i;
        }
        chunk.num_rows = 10;
        col.append_chunk(std::move(chunk));
    }
    sm.write_column("stream", col);

    auto reader = sm.open_column_reader("stream", "x");
    size_t total = 0;
    while (auto chunk = reader->read_next_chunk()) {
        total += chunk->num_rows;
    }
    EXPECT_EQ(total, 30u);
}
