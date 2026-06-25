# Columnar Analytics Engine

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

## Project Layout

See `IMPLEMENTATION.md` for the full implementation spec and phase-by-phase guide.

## License

MIT
