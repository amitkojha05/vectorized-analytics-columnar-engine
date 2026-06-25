#include "filter.hpp"
#include "batch.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

template<typename T>
bool compare(T lhs, T rhs, CompareOp op) {
    switch (op) {
        case CompareOp::EQ:  return lhs == rhs;
        case CompareOp::NEQ: return lhs != rhs;
        case CompareOp::LT:  return lhs < rhs;
        case CompareOp::LTE: return lhs <= rhs;
        case CompareOp::GT:  return lhs > rhs;
        case CompareOp::GTE: return lhs >= rhs;
    }
    return false;
}

template<typename T>
size_t eval_batch(const T* data, size_t batch_len, size_t base_idx, T rhs,
                    CompareOp op, const ColumnChunk* chunk,
                    SelectionVector& sel) {
    size_t added = 0;
    for (size_t j = 0; j < batch_len; ++j) {
        const size_t i = base_idx + j;
        if (chunk && chunk->has_nulls && chunk->is_null(i)) {
            continue;
        }
        if (compare(data[j], rhs, op)) {
            sel.sel[sel.count + added] = static_cast<uint16_t>(i);
            ++added;
        }
    }
    return added;
}

template<typename T>
size_t eval_typed(const ColumnChunk& chunk, const Predicate& pred,
                  const SelectionVector* input, SelectionVector& sel) {
    const T* data = chunk.values<T>();
    const T rhs   = [&]() {
        switch (chunk.type) {
            case DataType::INT32:   return static_cast<T>(pred.value.i32);
            case DataType::INT64:   return static_cast<T>(pred.value.i64);
            case DataType::FLOAT32: return static_cast<T>(pred.value.f32);
            case DataType::FLOAT64: return static_cast<T>(pred.value.f64);
            default:                return T{};
        }
    }();

    sel.count = 0;
    if (input) {
        for (size_t j = 0; j < input->count; ++j) {
            const size_t i = input->sel[j];
            if (chunk.has_nulls && chunk.is_null(i)) {
                continue;
            }
            const bool pass = compare(data[i], rhs, pred.op);
            if (pass) {
                sel.sel[sel.count] = static_cast<uint16_t>(i);
                ++sel.count;
            }
        }
    } else {
        const size_t n = chunk.num_rows;
        for (size_t i = 0; i < n; i += BATCH_SIZE) {
            const size_t end       = std::min(i + BATCH_SIZE, n);
            const size_t batch_len = end - i;
            sel.count += eval_batch(data + i, batch_len, i, rhs, pred.op, &chunk, sel);
        }
    }
    return sel.count;
}

int64_t predicate_threshold(const Predicate& pred, DataType type) {
    switch (type) {
        case DataType::INT32:   return pred.value.i32;
        case DataType::INT64:   return pred.value.i64;
        case DataType::FLOAT32: return static_cast<int64_t>(pred.value.f32);
        case DataType::FLOAT64: return static_cast<int64_t>(pred.value.f64);
        default:                return 0;
    }
}

}  // namespace

size_t evaluate_predicate(const ColumnChunk& chunk,
                          const Predicate& pred,
                          const SelectionVector* input,
                          SelectionVector& sel) {
    switch (chunk.type) {
        case DataType::INT32:
            return eval_typed<int32_t>(chunk, pred, input, sel);
        case DataType::INT64:
            return eval_typed<int64_t>(chunk, pred, input, sel);
        case DataType::FLOAT32:
            return eval_typed<float>(chunk, pred, input, sel);
        case DataType::FLOAT64:
            return eval_typed<double>(chunk, pred, input, sel);
        default:
            sel.count = 0;
            return 0;
    }
}

void SelectionVector::fill_all(size_t num_rows) {
    count = num_rows;
    for (size_t i = 0; i < num_rows; ++i) {
        sel[i] = static_cast<uint16_t>(i);
    }
}

size_t evaluate_predicate(const ColumnChunk& chunk,
                          const Predicate& pred,
                          SelectionVector& sel) {
    return evaluate_predicate(chunk, pred, nullptr, sel);
}

size_t evaluate_predicate(const ColumnChunk& chunk,
                          const Predicate& pred,
                          const SelectionVector& input,
                          SelectionVector& out) {
    return evaluate_predicate(chunk, pred, &input, out);
}


void evaluate_predicates(const std::vector<ColumnChunk>& chunks,
                         const std::vector<std::string>& col_names,
                         const std::vector<Predicate>& preds,
                         SelectionVector& sel) {
    if (preds.empty()) {
        if (!chunks.empty()) {
            sel.fill_all(chunks[0].num_rows);
        }
        return;
    }

    std::vector<Predicate> ordered = preds;
    std::sort(ordered.begin(), ordered.end(),
              [](const Predicate& a, const Predicate& b) {
                  return a.selectivity_hint < b.selectivity_hint;
              });

    SelectionVector next;
    const SelectionVector* input = nullptr;

    for (size_t p = 0; p < ordered.size(); ++p) {
        const auto& pred = ordered[p];
        const ColumnChunk* chunk = nullptr;
        for (size_t i = 0; i < col_names.size(); ++i) {
            if (col_names[i] == pred.col_name) {
                chunk = &chunks[i];
                break;
            }
        }
        if (!chunk) {
            sel.count = 0;
            return;
        }

        if (!chunk->stats.initialized) {
            const_cast<ColumnChunk*>(chunk)->finalize_stats();
        }

        const int64_t threshold = predicate_threshold(pred, chunk->type);
        if (chunk->can_skip_predicate(pred.op, threshold)) {
            sel.count = 0;
            return;
        }

        if (p == 0) {
            evaluate_predicate(*chunk, pred, sel);
            input = &sel;
        } else {
            evaluate_predicate(*chunk, pred, *input, next);
            sel = next;
            input = &sel;
        }
    }
}

void scan_gt_int32(const int32_t* data, size_t n, int32_t threshold,
                   SelectionVector& sel) {
    sel.count = 0;
    for (size_t i = 0; i < n; i += BATCH_SIZE) {
        const size_t end       = std::min(i + BATCH_SIZE, n);
        const int32_t* batch   = data + i;
        const size_t batch_len = end - i;

        for (size_t j = 0; j < batch_len; ++j) {
            if (batch[j] > threshold) {
                sel.sel[sel.count] = static_cast<uint16_t>(i + j);
                ++sel.count;
            }
        }
    }
}
