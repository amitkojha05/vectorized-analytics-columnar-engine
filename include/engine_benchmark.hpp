#pragma once
#include <chrono>
#include <functional>
#include <string>

struct BenchmarkResult {
    std::string name;
    double      time_ms;
    size_t      rows_processed;
    double      rows_per_sec;
};

class Timer {
public:
    void start() { start_ = std::chrono::steady_clock::now(); }

    double stop_ms() {
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

BenchmarkResult run_benchmark(const std::string& name,
                              std::function<void()> fn,
                              size_t rows);

void print_benchmark_result(const BenchmarkResult& result);
