#include "engine_benchmark.hpp"
#include <iostream>

BenchmarkResult run_benchmark(const std::string& name,
                              std::function<void()> fn,
                              size_t rows) {
    Timer t;
    t.start();
    fn();
    const double ms = t.stop_ms();
    const double rows_per_sec =
        ms > 0.0 ? (static_cast<double>(rows) / ms) * 1000.0 : 0.0;
    return {name, ms, rows, rows_per_sec};
}

void print_benchmark_result(const BenchmarkResult& result) {
    std::cout << result.name << "\n"
              << "  time_ms: " << result.time_ms << "\n"
              << "  rows: " << result.rows_processed << "\n"
              << "  rows/sec: " << result.rows_per_sec << "\n";
}
