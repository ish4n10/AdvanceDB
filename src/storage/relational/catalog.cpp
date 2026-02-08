#include "storage/relational/catalog.hpp"

using namespace Relational;

bool Catalog::register_table(const std::string& table_name, const TableSchema& schema) {
    
    auto pair = std::make_pair(table_name, schema);

    if (tables.find(table_name) != tables.end()) {
        return false;
    }
    tables.insert(pair);
    return true;
}

std::optional<const TableSchema*> Catalog::get_schema(const std::string& table_name) const {
    auto found_pair = tables.find(table_name);
    if (found_pair == tables.end()) {
        return std::nullopt;
    }

    return &found_pair->second;
}

bool Catalog::has_table(const std::string& table_name) const {
    if (tables.find(table_name) != tables.end()) {
        return false;
    }
    return true;
}

bool Catalog::drop_table(const std::string& table_name) {
    auto found_pair = tables.find(table_name);
    if (found_pair == tables.end()) {
        return false;
    }

    tables.erase(found_pair);
    return true;
}

