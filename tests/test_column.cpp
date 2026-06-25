#include "schema.hpp"
#include "column.hpp"
#include <gtest/gtest.h>

TEST(SchemaTest, ColumnIndexRoundTrip) {
    Schema schema;
    schema.table_name = "orders";
    schema.columns = {
        {"order_id", DataType::INT64},
        {"amount", DataType::FLOAT64},
        {"status", DataType::INT32},
    };

    EXPECT_EQ(schema.column_index("order_id"), 0);
    EXPECT_EQ(schema.column_index("amount"), 1);
    EXPECT_EQ(schema.column_index("status"), 2);
    EXPECT_EQ(schema.column_index("missing"), -1);
    EXPECT_EQ(schema.column_type("status"), DataType::INT32);
}

TEST(ColumnChunkTest, Int32RoundTrip) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(4);

    auto* data = chunk.values<int32_t>();
    for (int i = 0; i < 4; ++i) {
        data[i] = i * 10;
    }
    chunk.num_rows = 4;

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(chunk.values<int32_t>()[i], i * 10);
    }

    chunk.set_null(1, true);
    chunk.set_null(3, true);
    EXPECT_TRUE(chunk.is_null(1));
    EXPECT_TRUE(chunk.is_null(3));
    EXPECT_FALSE(chunk.is_null(0));
    EXPECT_FALSE(chunk.is_null(2));

    chunk.set_null(1, false);
    EXPECT_FALSE(chunk.is_null(1));
}

TEST(ColumnTest, AppendChunks) {
    Column col;
    col.def = {"value", DataType::INT32};

    ColumnChunk c1;
    c1.type = DataType::INT32;
    c1.resize(2);
    c1.values<int32_t>()[0] = 1;
    c1.values<int32_t>()[1] = 2;
    c1.num_rows = 2;
    col.append_chunk(std::move(c1));

    ColumnChunk c2;
    c2.type = DataType::INT32;
    c2.resize(3);
    c2.values<int32_t>()[0] = 3;
    c2.values<int32_t>()[1] = 4;
    c2.values<int32_t>()[2] = 5;
    c2.num_rows = 3;
    col.append_chunk(std::move(c2));

    EXPECT_EQ(col.total_rows(), 5u);
}

TEST(ColumnChunkTest, Alignment) {
    ColumnChunk chunk;
    chunk.type = DataType::INT64;
    chunk.resize(100);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(chunk.data.data()) % 64, 0u);
}
