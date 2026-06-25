#pragma once
#include "aggregator.hpp"
#include "filter.hpp"
#include "storage.hpp"
#include <optional>
#include <string>
#include <vector>

struct QueryPlan {
    std::string              table_name;
    std::vector<std::string> select_cols;
    std::vector<Predicate>   where;
    std::vector<AggExpr>     aggregates;
    std::optional<size_t>    limit;
};

struct QueryResult {
    std::vector<std::string>         col_names;
    std::vector<std::vector<double>> rows;
    std::vector<AggResult>           agg_results;
    size_t                           rows_scanned = 0;
    double                           elapsed_ms   = 0;
};

class QueryExecutor {
public:
    explicit QueryExecutor(StorageManager& sm);

    QueryResult execute(const QueryPlan& plan);

private:
    StorageManager& sm_;
};
