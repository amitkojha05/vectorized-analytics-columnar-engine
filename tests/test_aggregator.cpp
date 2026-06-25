#include "aggregator.hpp"
#include "column.hpp"
#include <gtest/gtest.h>

TEST(AggregatorTest, SumInt32) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(5);
    for (int i = 0; i < 5; ++i) {
        chunk.values<int32_t>()[i] = i + 1;
    }
    chunk.num_rows = 5;

    SelectionVector sel;
    sel.fill_all(5);

    Aggregator agg({{AggFunc::SUM, "value", "total"}});
    agg.accumulate({chunk}, {"value"}, sel);
    auto results = agg.finalize();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].value, 15.0);
}

TEST(AggregatorTest, CountStar) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(100);
    chunk.num_rows = 100;

    SelectionVector sel;
    sel.fill_all(100);

    Aggregator agg({{AggFunc::COUNT, "", "cnt"}});
    agg.accumulate({chunk}, {"value"}, sel);
    auto results = agg.finalize();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].value, 100.0);
}

TEST(AggregatorTest, MinMaxAvg) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(4);
    chunk.values<int32_t>()[0] = 10;
    chunk.values<int32_t>()[1] = 20;
    chunk.values<int32_t>()[2] = 30;
    chunk.values<int32_t>()[3] = 40;
    chunk.num_rows = 4;

    SelectionVector sel;
    sel.fill_all(4);

    Aggregator agg({
        {AggFunc::MIN, "v", "min_v"},
        {AggFunc::MAX, "v", "max_v"},
        {AggFunc::AVG, "v", "avg_v"},
    });
    agg.accumulate({chunk}, {"v"}, sel);
    auto results = agg.finalize();

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].value, 10.0);
    EXPECT_EQ(results[1].value, 40.0);
    EXPECT_DOUBLE_EQ(results[2].value, 25.0);
}

TEST(AggregatorTest, VectorizedSum) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(8);
    for (int i = 0; i < 8; ++i) {
        chunk.values<int32_t>()[i] = 1;
    }
    chunk.num_rows = 8;

    SelectionVector sel;
    sel.fill_all(8);

    EXPECT_EQ(vectorized_sum_int32(chunk.values<int32_t>(), sel), 8);
}

TEST(AggregatorTest, FilteredSum) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(4);
    chunk.values<int32_t>()[0] = 10;
    chunk.values<int32_t>()[1] = 20;
    chunk.values<int32_t>()[2] = 30;
    chunk.values<int32_t>()[3] = 40;
    chunk.num_rows = 4;

    SelectionVector sel;
    sel.count = 2;
    sel.sel[0] = 1;
    sel.sel[1] = 3;

    Aggregator agg({{AggFunc::SUM, "v", "total"}});
    agg.accumulate({chunk}, {"v"}, sel);
    auto results = agg.finalize();
    EXPECT_EQ(results[0].value, 60.0);
}
