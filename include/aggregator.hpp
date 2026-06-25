#pragma once
#include "filter.hpp"
#include <cstdint>
#include <string>
#include <vector>

enum class AggFunc { COUNT, SUM, MIN, MAX, AVG };

struct AggExpr {
    AggFunc     func;
    std::string col_name;
    std::string alias;
};

struct AggResult {
    std::string col_name;
    AggFunc     func;
    double      value;
    int64_t     count;
};

class Aggregator {
public:
    explicit Aggregator(std::vector<AggExpr> exprs);

    void accumulate(const std::vector<ColumnChunk>& cols,
                    const std::vector<std::string>& col_names,
                    const SelectionVector& sel);

    std::vector<AggResult> finalize();

private:
    struct State {
        AggExpr expr;
        double  value = 0;
        int64_t count = 0;
        bool    initialized = false;
    };

    void accumulate_one(State& st, const ColumnChunk& chunk,
                        const SelectionVector& sel);

    std::vector<State> states_;
};

int64_t vectorized_sum_int32(const int32_t* data, const SelectionVector& sel);
