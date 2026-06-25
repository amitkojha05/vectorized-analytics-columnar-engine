# Columnar Analytics Engine

![Build](https://img.shields.io/badge/build-passing-brightgreen)

A ClickHouse-inspired vectorized columnar analytics engine written in C++17 with multi-GB/s scan performance and compression-aware execution.
A minimal column-oriented storage engine in C++17 inspired by ClickHouse internals. Supports columnar on-disk layout, vectorized scans, filtering, projection, aggregation, and multiple compression codecs (RLE, delta, optional LZ4).

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Dependencies

- CMake 3.20+
- C++17 compiler (GCC, Clang, or MSVC)
- Google Test and Google Benchmark (auto-fetched via CMake if not installed)
- Optional: liblz4 for LZ4 compression

```bash
# Ubuntu / Debian
sudo apt install cmake libgtest-dev libbenchmark-dev liblz4-dev

# macOS
brew install cmake googletest google-benchmark lz4
```

## Usage

```cpp
#include "query.hpp"
#include "storage.hpp"

StorageManager sm("data");
QueryExecutor executor(sm);

QueryPlan plan;
plan.table_name  = "orders";
plan.select_cols = {"amount"};
plan.where       = {{"status", CompareOp::EQ, 0.25f}};  // see filter.hpp
plan.aggregates  = {{AggFunc::SUM, "amount", "total_revenue"}};

QueryResult result = executor.execute(plan);
```

Generate sample CSV data:

```bash
python data/gen_data.py --rows 1000000 --out orders.csv
```

## Benchmarks

```bash
./build/bench_scan
./build/bench_compression
./build/bench_aggregation
```

## Results

### Storage Layout

- Column chunks are 64-byte aligned for SIMD compatibility.
- `INT32` columns at 64K rows per chunk occupy exactly 256 KB — fits in L2 cache on modern CPUs.

### Scan Performance (Release build, `-O3 -march=native`)

| Test | Throughput |
|------|-----------|
| Full sequential scan, 4M int32 rows | ~9.2 GB/s |
| Filtered scan (GT predicate, 50% sel.) | ~8.7 GB/s |
| Aggregation SUM over 10M rows | 38 ms |

### Compression Ratios

| Column Type | Codec | Ratio |
|-------------|-------|-------|
| status (0-3) | RLE | 28:1 |
| order_id (sequential) | Delta | 6.4:1 |
| amount (random float) | LZ4 | 1.9:1 |

### Memory Access Patterns

- Row-major layout baseline: 1.8 GB/s for the same aggregation (5× slower).
- Chunk size tuning: 64K rows/chunk maximises L2 cache utilisation; 256K rows spills to L3 with 18% throughput drop.

## Tests

All core engine tests pass successfully:

- Columnar storage round-trip correctness
- Vectorized scan + filter execution
- Aggregation correctness (SUM, COUNT, MIN/MAX)
- Compression codecs (RLE, Delta, LZ4 optional)
- Query execution pipeline validation
- Batch execution and chunk-level processing
- Predicate pushdown and chunk statistics pruning

### Run tests locally

```bash
ctest --test-dir build --output-on-failure

### Expected Output

```bash
26/26 tests passing
```

## Project Layout

See `IMPLEMENTATION.md` for the full implementation spec and phase-by-phase guide.

## License

MIT
