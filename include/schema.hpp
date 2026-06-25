#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class DataType { INT32, INT64, FLOAT32, FLOAT64, STRING };

struct ColumnDef {
    std::string name;
    DataType    type;
};

struct Schema {
    std::string            table_name;
    std::vector<ColumnDef> columns;

    int column_index(const std::string& name) const;
    DataType column_type(const std::string& name) const;
};

size_t element_size(DataType type);
const char* data_type_name(DataType type);
DataType data_type_from_name(const std::string& name);
