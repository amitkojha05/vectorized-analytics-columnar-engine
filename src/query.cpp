#include "query.hpp"
#include "execution_plan.hpp"
#include "scan.hpp"
#include <algorithm>
#include <chrono>

QueryExecutor::QueryExecutor(StorageManager& sm) : sm_(sm) {}

QueryResult QueryExecutor::execute(const QueryPlan& plan) {
    const auto start = std::chrono::steady_clock::now();

    QueryResult result;

    ScanConfig scan_cfg;
    scan_cfg.table_name = plan.table_name;
    scan_cfg.filters    = plan.where;

    if (!plan.aggregates.empty()) {
        for (const auto& agg : plan.aggregates) {
            if (!agg.col_name.empty()) {
                scan_cfg.projection.push_back(agg.col_name);
            }
        }
        for (const auto& pred : plan.where) {
            if (std::find(scan_cfg.projection.begin(), scan_cfg.projection.end(),
                          pred.col_name) == scan_cfg.projection.end()) {
                scan_cfg.projection.push_back(pred.col_name);
            }
        }
    } else {
        scan_cfg.projection = plan.select_cols;
        for (const auto& pred : plan.where) {
            if (std::find(scan_cfg.projection.begin(), scan_cfg.projection.end(),
                          pred.col_name) == scan_cfg.projection.end()) {
                scan_cfg.projection.push_back(pred.col_name);
            }
        }
    }

    ScanOperator scan(sm_, scan_cfg);
    std::vector<ColumnChunk> batch;
    SelectionVector sel;
    Aggregator agg(plan.aggregates);

    PipelineExecutor pipeline;

    if (!plan.aggregates.empty()) {
        pipeline.nodes.push_back(
            {NodeType::SCAN,
             [&]() {
                 while (scan.next_batch(batch, sel)) {
                     result.rows_scanned += batch.empty() ? 0 : batch[0].num_rows;
                     agg.accumulate(batch, scan_cfg.projection, sel);
                 }
             }});

        pipeline.nodes.push_back({NodeType::AGGREGATE, [&]() {
                                      result.agg_results = agg.finalize();
                                      for (const auto& r : result.agg_results) {
                                          result.col_names.push_back(r.col_name);
                                      }
                                  }});
    } else {
        pipeline.nodes.push_back(
            {NodeType::SCAN,
             [&]() {
                 size_t rows_returned = 0;
                 while (scan.next_batch(batch, sel)) {
                     result.rows_scanned += batch.empty() ? 0 : batch[0].num_rows;

                     for (size_t s = 0; s < sel.count; ++s) {
                         if (plan.limit && rows_returned >= *plan.limit) {
                             break;
                         }
                         const size_t row = sel.sel[s];
                         std::vector<double> out_row;
                         out_row.reserve(plan.select_cols.size());
                         for (const auto& col_name : plan.select_cols) {
                             const size_t idx = static_cast<size_t>(
                                 std::find(scan_cfg.projection.begin(),
                                           scan_cfg.projection.end(),
                                           col_name) -
                                 scan_cfg.projection.begin());
                             const auto& chunk = batch[idx];
                             switch (chunk.type) {
                                 case DataType::INT32:
                                     out_row.push_back(static_cast<double>(
                                         chunk.values<int32_t>()[row]));
                                     break;
                                 case DataType::INT64:
                                     out_row.push_back(static_cast<double>(
                                         chunk.values<int64_t>()[row]));
                                     break;
                                 case DataType::FLOAT32:
                                     out_row.push_back(static_cast<double>(
                                         chunk.values<float>()[row]));
                                     break;
                                 case DataType::FLOAT64:
                                     out_row.push_back(chunk.values<double>()[row]);
                                     break;
                                 default:
                                     out_row.push_back(0.0);
                                     break;
                             }
                         }
                         result.rows.push_back(std::move(out_row));
                         ++rows_returned;
                     }
                     if (plan.limit && rows_returned >= *plan.limit) {
                         break;
                     }
                 }
                 result.col_names = plan.select_cols;
             }});
    }

    pipeline.execute();

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}
