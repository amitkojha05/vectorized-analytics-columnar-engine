#include "batch.hpp"
#include "column.hpp"
#include "engine_benchmark.hpp"
#include "filter.hpp"
#include <gtest/gtest.h>

TEST(BatchTest, FilterGtInt32Batch) {
    std::vector<int32_t> data = {0, 1, 5, 6, 3, 10, 2};
    auto matches = filter_gt_int32_batch(data.data(), data.size(), 4);
    ASSERT_EQ(matches.size(), 3u);
    EXPECT_EQ(matches[0], 2u);
    EXPECT_EQ(matches[1], 3u);
    EXPECT_EQ(matches[2], 5u);
}

TEST(ChunkStatsTest, MinMaxComputed) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(5);
    chunk.values<int32_t>()[0] = 10;
    chunk.values<int32_t>()[1] = 20;
    chunk.values<int32_t>()[2] = 5;
    chunk.values<int32_t>()[3] = 100;
    chunk.values<int32_t>()[4] = 30;
    chunk.num_rows             = 5;
    chunk.finalize_stats();

    EXPECT_TRUE(chunk.stats.initialized);
    EXPECT_EQ(chunk.stats.min, 5);
    EXPECT_EQ(chunk.stats.max, 100);
}

TEST(ChunkStatsTest, CanSkipGt) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(3);
    chunk.values<int32_t>()[0] = 1;
    chunk.values<int32_t>()[1] = 2;
    chunk.values<int32_t>()[2] = 3;
    chunk.num_rows             = 3;
    chunk.finalize_stats();

    EXPECT_TRUE(chunk.can_skip_gt(10));
    EXPECT_FALSE(chunk.can_skip_gt(2));
}

TEST(ChunkStatsTest, MightMatchRange) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(3);
    chunk.values<int32_t>()[0] = 10;
    chunk.values<int32_t>()[1] = 20;
    chunk.values<int32_t>()[2] = 30;
    chunk.num_rows             = 3;
    chunk.finalize_stats();

    EXPECT_TRUE(chunk.might_match_range(15, 25));
    EXPECT_FALSE(chunk.might_match_range(50, 60));
}

TEST(ChunkStatsTest, PredicatePruning) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(3);
    chunk.values<int32_t>()[0] = 1;
    chunk.values<int32_t>()[1] = 2;
    chunk.values<int32_t>()[2] = 3;
    chunk.num_rows             = 3;
    chunk.finalize_stats();

    EXPECT_TRUE(chunk.can_skip_predicate(CompareOp::GT, 10));
    EXPECT_TRUE(chunk.can_skip_predicate(CompareOp::LT, 1));
    EXPECT_FALSE(chunk.can_skip_predicate(CompareOp::GT, 2));
}

TEST(PipelineTest, BenchmarkHarness) {
    int counter = 0;
    auto result = run_benchmark(
        "noop",
        [&]() {
            for (int i = 0; i < 1000; ++i) {
                ++counter;
            }
        },
        1000);
    EXPECT_EQ(result.name, "noop");
    EXPECT_EQ(result.rows_processed, 1000u);
    EXPECT_GT(result.time_ms, 0.0);
    EXPECT_GT(result.rows_per_sec, 0.0);
    EXPECT_EQ(counter, 1000);
}
