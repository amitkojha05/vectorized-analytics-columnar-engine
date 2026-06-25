#include "query.hpp"
#include "storage.hpp"
#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class QueryTest : public ::testing::Test {
protected:
    std::string tmp_dir = "test_data_query";

    void SetUp() override {
        fs::remove_all(tmp_dir);
        setup_orders();
    }

    void TearDown() override { fs::remove_all(tmp_dir); }

    void setup_orders() {
        StorageManager sm(tmp_dir);
        Schema schema;
        schema.table_name = "orders";
        schema.columns    = {
            {"amount", DataType::FLOAT64},
            {"status", DataType::INT32},
        };
        sm.create_table(schema);

        Column amount_col;
        amount_col.def = {"amount", DataType::FLOAT64};
        Column status_col;
        status_col.def = {"status", DataType::INT32};

        ColumnChunk amount;
        amount.type = DataType::FLOAT64;
        amount.resize(4);
        amount.values<double>()[0] = 100.0;
        amount.values<double>()[1] = 200.0;
        amount.values<double>()[2] = 300.0;
        amount.values<double>()[3] = 400.0;
        amount.num_rows = 4;

        ColumnChunk status;
        status.type = DataType::INT32;
        status.resize(4);
        status.values<int32_t>()[0] = 1;
        status.values<int32_t>()[1] = 0;
        status.values<int32_t>()[2] = 1;
        status.values<int32_t>()[3] = 1;
        status.num_rows = 4;

        amount_col.append_chunk(std::move(amount));
        status_col.append_chunk(std::move(status));

        sm.write_column("orders", amount_col);
        sm.write_column("orders", status_col);
    }
};

TEST_F(QueryTest, AggregateSumWithFilter) {
    StorageManager sm(tmp_dir);
    QueryExecutor executor(sm);

    QueryPlan plan;
    plan.table_name = "orders";
    plan.select_cols = {"amount"};
    Predicate pred;
    pred.col_name = "status";
    pred.op = CompareOp::EQ;
    pred.selectivity_hint = 0.25f;
    pred.value.i32 = 1;
    plan.where = {pred};
    plan.aggregates = {{AggFunc::SUM, "amount", "total_revenue"}};

    QueryResult result = executor.execute(plan);
    ASSERT_EQ(result.agg_results.size(), 1u);
    EXPECT_DOUBLE_EQ(result.agg_results[0].value, 800.0);
}

TEST_F(QueryTest, RawProjection) {
    StorageManager sm(tmp_dir);
    QueryExecutor executor(sm);

    QueryPlan plan;
    plan.table_name  = "orders";
    plan.select_cols = {"amount"};
    plan.limit       = 2;

    QueryResult result = executor.execute(plan);
    EXPECT_EQ(result.rows.size(), 2u);
    EXPECT_DOUBLE_EQ(result.rows[0][0], 100.0);
    EXPECT_DOUBLE_EQ(result.rows[1][0], 200.0);
}
