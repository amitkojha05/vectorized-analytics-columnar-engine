#include "schema.hpp"
#include <stdexcept>

size_t element_size(DataType type) {
    switch (type) {
        case DataType::INT32:   return 4;
        case DataType::INT64:   return 8;
        case DataType::FLOAT32: return 4;
        case DataType::FLOAT64: return 8;
        case DataType::STRING:  return sizeof(uint32_t);
    }
    return 0;
}

const char* data_type_name(DataType type) {
    switch (type) {
        case DataType::INT32:   return "INT32";
        case DataType::INT64:   return "INT64";
        case DataType::FLOAT32: return "FLOAT32";
        case DataType::FLOAT64: return "FLOAT64";
        case DataType::STRING:  return "STRING";
    }
    return "UNKNOWN";
}

DataType data_type_from_name(const std::string& name) {
    if (name == "INT32")   return DataType::INT32;
    if (name == "INT64")   return DataType::INT64;
    if (name == "FLOAT32") return DataType::FLOAT32;
    if (name == "FLOAT64") return DataType::FLOAT64;
    if (name == "STRING")  return DataType::STRING;
    throw std::invalid_argument("unknown data type: " + name);
}

int Schema::column_index(const std::string& name) const {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

DataType Schema::column_type(const std::string& name) const {
    int idx = column_index(name);
    if (idx < 0) {
        throw std::invalid_argument("unknown column: " + name);
    }
    return columns[static_cast<size_t>(idx)].type;
}
