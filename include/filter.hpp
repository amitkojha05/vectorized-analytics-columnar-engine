#pragma once
#include "column.hpp"
#include <array>
#include <string>
#include <vector>

enum class CompareOp { EQ, NEQ, LT, LTE, GT, GTE };

struct Predicate {
    std::string col_name;
    CompareOp   op;
    float       selectivity_hint = 1.0f;

    union Value {
        int32_t  i32;
        int64_t  i64;
        float    f32;
        double   f64;
    } value;
};

struct SelectionVector {
    std::array<uint16_t, CHUNK_SIZE> sel{};
    size_t count = 0;

    void fill_all(size_t num_rows);
    void reset() { count = 0; }
};

size_t evaluate_predicate(const ColumnChunk& chunk,
                          const Predicate& pred,
                          SelectionVector& sel);

size_t evaluate_predicate(const ColumnChunk& chunk,
                          const Predicate& pred,
                          const SelectionVector& input,
                          SelectionVector& out);

void evaluate_predicates(const std::vector<ColumnChunk>& chunks,
                         const std::vector<std::string>& col_names,
                         const std::vector<Predicate>& preds,
                         SelectionVector& sel);
