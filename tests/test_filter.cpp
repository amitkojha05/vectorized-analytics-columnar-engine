#include "filter.hpp"
#include "column.hpp"
#include <gtest/gtest.h>

TEST(FilterTest, GtInt32) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(10);
    auto* data = chunk.values<int32_t>();
    for (int i = 0; i < 10; ++i) {
        data[i] = i;
    }
    chunk.num_rows = 10;

    Predicate pred;
    pred.col_name = "value";
    pred.op = CompareOp::GT;
    pred.selectivity_hint = 1.0f;
    pred.value.i32 = 5;

    SelectionVector sel;
    evaluate_predicate(chunk, pred, sel);
    EXPECT_EQ(sel.count, 4u);
    for (size_t i = 0; i < sel.count; ++i) {
        EXPECT_GT(data[sel.sel[i]], 5);
    }
}

TEST(FilterTest, EqInt32) {
    ColumnChunk chunk;
    chunk.type = DataType::INT32;
    chunk.resize(5);
    chunk.values<int32_t>()[0] = 1;
    chunk.values<int32_t>()[1] = 2;
    chunk.values<int32_t>()[2] = 2;
    chunk.values<int32_t>()[3] = 3;
    chunk.values<int32_t>()[4] = 2;
    chunk.num_rows = 5;

    Predicate pred;
    pred.col_name = "value";
    pred.op = CompareOp::EQ;
    pred.selectivity_hint = 0.5f;
    pred.value.i32 = 2;

    SelectionVector sel;
    evaluate_predicate(chunk, pred, sel);
    EXPECT_EQ(sel.count, 3u);
}

TEST(FilterTest, AndPredicates) {
    ColumnChunk status;
    status.type = DataType::INT32;
    status.resize(4);
    status.values<int32_t>()[0] = 1;
    status.values<int32_t>()[1] = 2;
    status.values<int32_t>()[2] = 1;
    status.values<int32_t>()[3] = 3;
    status.num_rows = 4;

    ColumnChunk amount;
    amount.type = DataType::INT32;
    amount.resize(4);
    amount.values<int32_t>()[0] = 100;
    amount.values<int32_t>()[1] = 50;
    amount.values<int32_t>()[2] = 200;
    amount.values<int32_t>()[3] = 300;
    amount.num_rows = 4;

    Predicate p1;
    p1.col_name = "status";
    p1.op = CompareOp::EQ;
    p1.selectivity_hint = 0.25f;
    p1.value.i32 = 1;
    Predicate p2;
    p2.col_name = "amount";
    p2.op = CompareOp::GT;
    p2.selectivity_hint = 0.5f;
    p2.value.i32 = 150;

    SelectionVector sel;
    evaluate_predicates({status, amount}, {"status", "amount"}, {p1, p2}, sel);
    EXPECT_EQ(sel.count, 1u);
    EXPECT_EQ(status.values<int32_t>()[sel.sel[0]], 1);
    EXPECT_EQ(amount.values<int32_t>()[sel.sel[0]], 200);
}

TEST(FilterTest, SelectionVectorFillAll) {
    SelectionVector sel;
    sel.fill_all(100);
    EXPECT_EQ(sel.count, 100u);
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(sel.sel[i], i);
    }
}
