#pragma once
#include "filter.hpp"
#include "storage.hpp"
#include <string>
#include <vector>

struct ScanConfig {
    std::string              table_name;
    std::vector<std::string> projection;
    std::vector<Predicate>   filters;
};

class ScanOperator {
public:
    ScanOperator(StorageManager& sm, ScanConfig cfg);

    bool next_batch(std::vector<ColumnChunk>& out_cols, SelectionVector& sel);

private:
    void apply_filters(std::vector<ColumnChunk>& cols, SelectionVector& sel);

    StorageManager&          sm_;
    ScanConfig               cfg_;
    size_t                   chunk_idx_ = 0;
    std::vector<std::string> col_names_;
    std::vector<Column>      columns_;
    bool                     initialized_ = false;
};

void scan_gt_int32(const int32_t* data, size_t n, int32_t threshold,
                   SelectionVector& sel);
