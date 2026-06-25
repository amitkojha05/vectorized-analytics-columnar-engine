#include "scan.hpp"
#include <algorithm>

namespace {

int64_t predicate_threshold(const Predicate& pred, DataType type) {
    switch (type) {
        case DataType::INT32:   return pred.value.i32;
        case DataType::INT64:   return pred.value.i64;
        case DataType::FLOAT32: return static_cast<int64_t>(pred.value.f32);
        case DataType::FLOAT64: return static_cast<int64_t>(pred.value.f64);
        default:                return 0;
    }
}

bool chunk_set_can_be_skipped(const std::vector<ColumnChunk>& cols,
                              const std::vector<std::string>& col_names,
                              const std::vector<Predicate>& preds) {
    for (const auto& pred : preds) {
        for (size_t i = 0; i < col_names.size(); ++i) {
            if (col_names[i] != pred.col_name) {
                continue;
            }
            const ColumnChunk& chunk = cols[i];
            if (!chunk.stats.initialized) {
                return false;
            }
            const int64_t threshold = predicate_threshold(pred, chunk.type);
            if (chunk.can_skip_predicate(pred.op, threshold)) {
                return true;
            }
            break;
        }
    }
    return false;
}

}  // namespace

ScanOperator::ScanOperator(StorageManager& sm, ScanConfig cfg)
    : sm_(sm), cfg_(std::move(cfg)) {}

bool ScanOperator::next_batch(std::vector<ColumnChunk>& out_cols,
                              SelectionVector& sel) {
    if (!initialized_) {
        Schema schema = sm_.load_schema(cfg_.table_name);
        if (cfg_.projection.empty()) {
            for (const auto& col : schema.columns) {
                col_names_.push_back(col.name);
            }
        } else {
            col_names_ = cfg_.projection;
        }

        columns_.resize(col_names_.size());
        for (size_t i = 0; i < col_names_.size(); ++i) {
            columns_[i] = sm_.read_column(cfg_.table_name, col_names_[i]);
        }
        initialized_ = true;
    }

    out_cols.clear();
    size_t min_chunks = SIZE_MAX;
    for (const auto& col : columns_) {
        min_chunks = std::min(min_chunks, col.chunks.size());
    }

    if (chunk_idx_ >= min_chunks) {
        return false;
    }

    out_cols.reserve(col_names_.size());
    for (auto& col : columns_) {
        out_cols.push_back(col.chunks[chunk_idx_]);
    }
    ++chunk_idx_;

    for (auto& chunk : out_cols) {
        if (!chunk.stats.initialized) {
            chunk.finalize_stats();
        }
    }

    if (!cfg_.filters.empty() &&
        chunk_set_can_be_skipped(out_cols, col_names_, cfg_.filters)) {
        sel.count = 0;
        return true;
    }

    apply_filters(out_cols, sel);
    return true;
}

void ScanOperator::apply_filters(std::vector<ColumnChunk>& cols,
                                 SelectionVector& sel) {
    if (cfg_.filters.empty()) {
        if (!cols.empty()) {
            sel.fill_all(cols[0].num_rows);
        }
        return;
    }
    evaluate_predicates(cols, col_names_, cfg_.filters, sel);
}
