#include "aggregator.hpp"

int64_t vectorized_sum_int32(const int32_t* data, const SelectionVector& sel) {
    int64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    size_t i = 0;
    for (; i + 4 <= sel.count; i += 4) {
        s0 += data[sel.sel[i]];
        s1 += data[sel.sel[i + 1]];
        s2 += data[sel.sel[i + 2]];
        s3 += data[sel.sel[i + 3]];
    }
    for (; i < sel.count; ++i) {
        s0 += data[sel.sel[i]];
    }
    return s0 + s1 + s2 + s3;
}

Aggregator::Aggregator(std::vector<AggExpr> exprs) {
    for (auto& expr : exprs) {
        State st;
        st.expr = std::move(expr);
        states_.push_back(std::move(st));
    }
}

void Aggregator::accumulate_one(State& st, const ColumnChunk& chunk,
                                const SelectionVector& sel) {
    switch (chunk.type) {
        case DataType::INT32: {
            const int32_t* data = chunk.values<int32_t>();
            switch (st.expr.func) {
                case AggFunc::COUNT:
                    st.count += static_cast<int64_t>(sel.count);
                    st.value = static_cast<double>(st.count);
                    break;
                case AggFunc::SUM:
                    st.value += static_cast<double>(vectorized_sum_int32(data, sel));
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::MIN:
                    for (size_t i = 0; i < sel.count; ++i) {
                        const double v = data[sel.sel[i]];
                        if (!st.initialized || v < st.value) {
                            st.value = v;
                            st.initialized = true;
                        }
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::MAX:
                    for (size_t i = 0; i < sel.count; ++i) {
                        const double v = data[sel.sel[i]];
                        if (!st.initialized || v > st.value) {
                            st.value = v;
                            st.initialized = true;
                        }
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::AVG:
                    for (size_t i = 0; i < sel.count; ++i) {
                        st.value += data[sel.sel[i]];
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
            }
            break;
        }
        case DataType::INT64: {
            const int64_t* data = chunk.values<int64_t>();
            switch (st.expr.func) {
                case AggFunc::COUNT:
                    st.count += static_cast<int64_t>(sel.count);
                    st.value = static_cast<double>(st.count);
                    break;
                case AggFunc::SUM:
                    for (size_t i = 0; i < sel.count; ++i) {
                        st.value += static_cast<double>(data[sel.sel[i]]);
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::MIN:
                    for (size_t i = 0; i < sel.count; ++i) {
                        const double v = static_cast<double>(data[sel.sel[i]]);
                        if (!st.initialized || v < st.value) {
                            st.value = v;
                            st.initialized = true;
                        }
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::MAX:
                    for (size_t i = 0; i < sel.count; ++i) {
                        const double v = static_cast<double>(data[sel.sel[i]]);
                        if (!st.initialized || v > st.value) {
                            st.value = v;
                            st.initialized = true;
                        }
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::AVG:
                    for (size_t i = 0; i < sel.count; ++i) {
                        st.value += static_cast<double>(data[sel.sel[i]]);
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
            }
            break;
        }
        case DataType::FLOAT64: {
            const double* data = chunk.values<double>();
            switch (st.expr.func) {
                case AggFunc::COUNT:
                    st.count += static_cast<int64_t>(sel.count);
                    st.value = static_cast<double>(st.count);
                    break;
                case AggFunc::SUM:
                    for (size_t i = 0; i < sel.count; ++i) {
                        st.value += data[sel.sel[i]];
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::MIN:
                    for (size_t i = 0; i < sel.count; ++i) {
                        const double v = data[sel.sel[i]];
                        if (!st.initialized || v < st.value) {
                            st.value = v;
                            st.initialized = true;
                        }
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::MAX:
                    for (size_t i = 0; i < sel.count; ++i) {
                        const double v = data[sel.sel[i]];
                        if (!st.initialized || v > st.value) {
                            st.value = v;
                            st.initialized = true;
                        }
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
                case AggFunc::AVG:
                    for (size_t i = 0; i < sel.count; ++i) {
                        st.value += data[sel.sel[i]];
                    }
                    st.count += static_cast<int64_t>(sel.count);
                    break;
            }
            break;
        }
        default:
            break;
    }
}

void Aggregator::accumulate(const std::vector<ColumnChunk>& cols,
                            const std::vector<std::string>& col_names,
                            const SelectionVector& sel) {
    for (auto& st : states_) {
        if (st.expr.func == AggFunc::COUNT && st.expr.col_name.empty()) {
            st.count += static_cast<int64_t>(sel.count);
            st.value = static_cast<double>(st.count);
            continue;
        }

        const ColumnChunk* target = nullptr;
        for (size_t i = 0; i < cols.size(); ++i) {
            if (col_names[i] == st.expr.col_name) {
                target = &cols[i];
                break;
            }
        }
        if (!target) {
            continue;
        }
        accumulate_one(st, *target, sel);
    }
}

std::vector<AggResult> Aggregator::finalize() {
    std::vector<AggResult> results;
    results.reserve(states_.size());

    for (const auto& st : states_) {
        AggResult r;
        r.col_name = st.expr.alias.empty() ? st.expr.col_name : st.expr.alias;
        r.func     = st.expr.func;
        r.count    = st.count;
        if (st.expr.func == AggFunc::AVG && st.count > 0) {
            r.value = st.value / static_cast<double>(st.count);
        } else {
            r.value = st.value;
        }
        results.push_back(r);
    }
    return results;
}
