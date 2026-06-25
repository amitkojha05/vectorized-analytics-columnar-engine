#pragma once
#include "column.hpp"
#include "compression.hpp"
#include "schema.hpp"
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct ChunkHeader {
    uint32_t magic     = 0x434F4C01;
    uint32_t num_rows;
    uint32_t data_size;
    uint32_t bitmap_size;
    uint8_t  codec;
    uint8_t  has_nulls;
    uint8_t  element_size;
    uint8_t  _pad;
};
#pragma pack(pop)

class ColumnReader;

class StorageManager {
public:
    explicit StorageManager(const std::string& data_dir);

    void   create_table(const Schema& schema);
    Schema load_schema(const std::string& table_name);
    void   write_column(const std::string& table, const Column& col,
                        Codec codec = Codec::NONE);
    Column read_column(const std::string& table, const std::string& col_name);

    std::unique_ptr<ColumnReader> open_column_reader(const std::string& table,
                                                     const std::string& col_name);

    void ingest_csv(const std::string& table, const std::string& csv_path);

    const std::string& data_dir() const { return data_dir_; }

private:
    std::string data_dir_;
};

class ColumnReader {
public:
    ColumnReader(std::string col_path, std::string idx_path, DataType type);

    std::optional<ColumnChunk> read_next_chunk();
    bool exhausted() const { return exhausted_; }

private:
    std::ifstream            file_;
    std::vector<std::pair<uint64_t, uint32_t>> index_;
    size_t                   next_idx_ = 0;
    DataType                 type_;
    bool                     exhausted_ = false;
};
