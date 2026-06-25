#include "storage.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace fs = std::filesystem;

namespace {

std::string table_path(const std::string& data_dir, const std::string& table) {
    return data_dir + "/" + table;
}

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += c;
        }
    }
    return out;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot write file: " + path);
    }
    out << content;
}

std::string extract_string_field(const std::string& json, const std::string& key,
                                 size_t& pos) {
    const std::string needle = "\"" + key + "\":";
    pos = json.find(needle, pos);
    if (pos == std::string::npos) {
        return {};
    }
    pos += needle.size();
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != '"') {
        return {};
    }
    ++pos;
    std::string value;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            value += json[pos + 1];
            pos += 2;
        } else {
            value += json[pos++];
        }
    }
    if (pos < json.size()) {
        ++pos;
    }
    return value;
}

Schema parse_schema_json(const std::string& json) {
    Schema schema;
    size_t pos = 0;
    schema.table_name = extract_string_field(json, "table_name", pos);

    const auto cols_pos = json.find("\"columns\"");
    if (cols_pos == std::string::npos) {
        return schema;
    }

    pos = cols_pos;
    while (true) {
        size_t name_pos = pos;
        std::string name = extract_string_field(json, "name", name_pos);
        if (name.empty()) {
            break;
        }
        size_t type_pos = name_pos;
        std::string type_str = extract_string_field(json, "type", type_pos);
        if (type_str.empty()) {
            break;
        }
        schema.columns.push_back({name, data_type_from_name(type_str)});
        pos = type_pos;
    }
    return schema;
}

void write_chunk(std::ostream& out, const ColumnChunk& chunk, Codec codec) {
    ChunkHeader hdr{};
    hdr.num_rows    = static_cast<uint32_t>(chunk.num_rows);
    hdr.has_nulls   = chunk.has_nulls ? 1 : 0;
    hdr.codec       = static_cast<uint8_t>(codec);
    hdr.element_size = static_cast<uint8_t>(element_size(chunk.type));

    const size_t raw_size = chunk.num_rows * element_size(chunk.type);
    std::vector<uint8_t> raw(raw_size);
    if (raw_size > 0) {
        std::memcpy(raw.data(), chunk.data.data(), raw_size);
    }

    std::vector<uint8_t> payload;
    if (codec == Codec::NONE) {
        payload = std::move(raw);
    } else {
        auto c = make_codec(codec);
        auto block = c->compress(raw.data(), raw.size(), hdr.element_size);
        payload = std::move(block.data);
    }

    hdr.data_size   = static_cast<uint32_t>(payload.size());
    hdr.bitmap_size = static_cast<uint32_t>(chunk.null_bitmap.size());

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!payload.empty()) {
        out.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size()));
    }
    if (!chunk.null_bitmap.empty()) {
        out.write(reinterpret_cast<const char*>(chunk.null_bitmap.data()),
                  static_cast<std::streamsize>(chunk.null_bitmap.size()));
    }
}

ColumnChunk read_chunk(std::istream& in, DataType type) {
    ChunkHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) {
        throw std::runtime_error("failed to read chunk header");
    }
    if (hdr.magic != 0x434F4C01) {
        throw std::runtime_error("invalid chunk magic");
    }

    ColumnChunk chunk;
    chunk.type     = type;
    chunk.num_rows = hdr.num_rows;
    chunk.has_nulls = hdr.has_nulls != 0;

    std::vector<uint8_t> payload(hdr.data_size);
    if (hdr.data_size > 0) {
        in.read(reinterpret_cast<char*>(payload.data()),
                static_cast<std::streamsize>(hdr.data_size));
    }

    std::vector<uint8_t> raw;
    const Codec codec = static_cast<Codec>(hdr.codec);
    if (codec == Codec::NONE) {
        raw = std::move(payload);
    } else {
        CompressedBlock block;
        block.codec         = codec;
        block.element_size  = hdr.element_size;
        block.original_size = hdr.num_rows * hdr.element_size;
        block.data          = std::move(payload);
        auto c = make_codec(codec);
        raw = c->decompress(block);
    }

    chunk.data.resize(raw.size(), 64);
    if (!raw.empty()) {
        std::memcpy(chunk.data.data(), raw.data(), raw.size());
    }

    if (hdr.bitmap_size > 0) {
        chunk.null_bitmap.resize(hdr.bitmap_size);
        in.read(reinterpret_cast<char*>(chunk.null_bitmap.data()),
                static_cast<std::streamsize>(hdr.bitmap_size));
    }

    chunk.finalize_stats();
    return chunk;
}

}  // namespace

StorageManager::StorageManager(const std::string& data_dir)
    : data_dir_(data_dir) {
    fs::create_directories(data_dir_);
}

void StorageManager::create_table(const Schema& schema) {
    const std::string dir = table_path(data_dir_, schema.table_name);
    fs::create_directories(dir);

    std::ostringstream json;
    json << "{\n";
    json << "  \"table_name\": \"" << escape_json(schema.table_name) << "\",\n";
    json << "  \"columns\": [\n";
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        const auto& col = schema.columns[i];
        json << "    {\"name\": \"" << escape_json(col.name)
             << "\", \"type\": \"" << data_type_name(col.type) << "\"}";
        if (i + 1 < schema.columns.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    write_file(dir + "/schema.json", json.str());
}

Schema StorageManager::load_schema(const std::string& table_name) {
    const std::string path = table_path(data_dir_, table_name) + "/schema.json";
    return parse_schema_json(read_file(path));
}

void StorageManager::write_column(const std::string& table, const Column& col,
                                  Codec codec) {
    const std::string dir = table_path(data_dir_, table);
    fs::create_directories(dir);

    const std::string col_path = dir + "/" + col.def.name + ".col";
    const std::string idx_path = dir + "/" + col.def.name + ".col.idx";

    std::ofstream out(col_path, std::ios::binary | std::ios::trunc);
    std::ofstream idx(idx_path, std::ios::binary | std::ios::trunc);

    for (const auto& chunk : col.chunks) {
        const uint64_t offset = static_cast<uint64_t>(out.tellp());
        write_chunk(out, chunk, codec);
        const uint32_t rows = static_cast<uint32_t>(chunk.num_rows);
        idx.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
        idx.write(reinterpret_cast<const char*>(&rows), sizeof(rows));
    }
}

Column StorageManager::read_column(const std::string& table,
                                   const std::string& col_name) {
    Schema schema = load_schema(table);
    const int idx = schema.column_index(col_name);
    if (idx < 0) {
        throw std::invalid_argument("unknown column: " + col_name);
    }

    Column col;
    col.def = schema.columns[static_cast<size_t>(idx)];

    auto reader = open_column_reader(table, col_name);
    while (true) {
        auto chunk = reader->read_next_chunk();
        if (!chunk) {
            break;
        }
        col.chunks.push_back(std::move(*chunk));
    }
    return col;
}

std::unique_ptr<ColumnReader> StorageManager::open_column_reader(
    const std::string& table, const std::string& col_name) {
    Schema schema = load_schema(table);
    const int idx = schema.column_index(col_name);
    if (idx < 0) {
        throw std::invalid_argument("unknown column: " + col_name);
    }

    const std::string dir = table_path(data_dir_, table);
    return std::make_unique<ColumnReader>(
        dir + "/" + col_name + ".col",
        dir + "/" + col_name + ".col.idx",
        schema.columns[static_cast<size_t>(idx)].type);
}

ColumnReader::ColumnReader(std::string col_path, std::string idx_path,
                           DataType type)
    : type_(type) {
    file_.open(col_path, std::ios::binary);
    if (!file_) {
        throw std::runtime_error("cannot open column file: " + col_path);
    }

    std::ifstream idx(idx_path, std::ios::binary);
    if (!idx) {
        throw std::runtime_error("cannot open index file: " + idx_path);
    }

    while (idx.peek() != EOF) {
        uint64_t offset = 0;
        uint32_t rows   = 0;
        idx.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        idx.read(reinterpret_cast<char*>(&rows), sizeof(rows));
        if (!idx) {
            break;
        }
        index_.push_back({offset, rows});
    }

    if (index_.empty()) {
        exhausted_ = true;
    }
}

std::optional<ColumnChunk> ColumnReader::read_next_chunk() {
    if (exhausted_ || next_idx_ >= index_.size()) {
        exhausted_ = true;
        return std::nullopt;
    }

    file_.seekg(static_cast<std::streamoff>(index_[next_idx_].first));
    ColumnChunk chunk = read_chunk(file_, type_);
    ++next_idx_;
    if (next_idx_ >= index_.size()) {
        exhausted_ = true;
    }
    return chunk;
}

void StorageManager::ingest_csv(const std::string& table,
                                const std::string& csv_path) {
    Schema schema = load_schema(table);
    std::ifstream in(csv_path);
    if (!in) {
        throw std::runtime_error("cannot open csv: " + csv_path);
    }

    std::string header_line;
    std::getline(in, header_line);

    std::vector<Column> columns(schema.columns.size());
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        columns[i].def = schema.columns[i];
    }

    std::vector<ColumnChunk> active(schema.columns.size());
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        active[i].type = schema.columns[i].type;
        active[i].resize(CHUNK_SIZE);
        active[i].num_rows = 0;
    }

    auto flush_chunk = [&](size_t col_idx) {
        active[col_idx].num_rows = CHUNK_SIZE;
        active[col_idx].finalize_stats();
        columns[col_idx].append_chunk(std::move(active[col_idx]));
        active[col_idx] = ColumnChunk{};
        active[col_idx].type = schema.columns[col_idx].type;
        active[col_idx].resize(CHUNK_SIZE);
        active[col_idx].num_rows = 0;
    };

    std::string line;
    size_t row_in_chunk = 0;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> fields;
        std::string field;
        bool in_quotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(field);

        if (fields.size() != schema.columns.size()) {
            continue;
        }

        for (size_t c = 0; c < schema.columns.size(); ++c) {
            const size_t row = row_in_chunk;
            switch (schema.columns[c].type) {
                case DataType::INT32:
                    *reinterpret_cast<int32_t*>(active[c].data.data() +
                        row * element_size(DataType::INT32)) =
                        std::stoi(fields[c]);
                    break;
                case DataType::INT64:
                    *reinterpret_cast<int64_t*>(active[c].data.data() +
                        row * element_size(DataType::INT64)) =
                        std::stoll(fields[c]);
                    break;
                case DataType::FLOAT32:
                    *reinterpret_cast<float*>(active[c].data.data() +
                        row * element_size(DataType::FLOAT32)) =
                        std::stof(fields[c]);
                    break;
                case DataType::FLOAT64:
                    *reinterpret_cast<double*>(active[c].data.data() +
                        row * element_size(DataType::FLOAT64)) =
                        std::stod(fields[c]);
                    break;
                default:
                    break;
            }
        }

        ++row_in_chunk;
        if (row_in_chunk == CHUNK_SIZE) {
            for (size_t c = 0; c < schema.columns.size(); ++c) {
                flush_chunk(c);
            }
            row_in_chunk = 0;
        }
    }

    if (row_in_chunk > 0) {
        for (size_t c = 0; c < schema.columns.size(); ++c) {
            active[c].num_rows = row_in_chunk;
            active[c].finalize_stats();
            columns[c].append_chunk(std::move(active[c]));
        }
    }

    for (auto& col : columns) {
        write_column(table, col);
    }
}
