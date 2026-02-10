#include "storage/interface/storage_engine.hpp"
#include "storage/relational/catalog.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>

#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << std::endl; return 1; } } while(0)

static void ensure_data_dir() {
    std::filesystem::create_directories("data");
}

int main() {
    ensure_data_dir();
    std::cout << "\n=== Relational Storage Engine Test ===" << std::endl;
    std::remove("data/test_relational_engine.db");

    StorageEngine engine;

    Relational::TableSchema schema;
    schema.pk_index = 0;
    schema.columns = {
        {"id", Relational::ColumnType::INT},
        {"name", Relational::ColumnType::STRING},
        {"age", Relational::ColumnType::INT}
    };

    CHECK(engine.create_table("test_relational_engine", schema), "create_table(name, schema) failed");
    std::cout << "[OK] create_table(name, schema)" << std::endl;

    CHECK(engine.has_table("test_relational_engine"), "has_table should be true");
    std::cout << "[OK] has_table" << std::endl;

    const Relational::TableSchema* got = engine.get_schema("test_relational_engine");
    CHECK(got != nullptr && got->columns.size() == 3 && got->pk_index == 0, "get_schema");
    std::cout << "[OK] get_schema" << std::endl;

    Relational::Tuple row1 = { 1, std::string("Alice"), 25 };
    CHECK(engine.insert("test_relational_engine", row1), "insert row1 failed");
    std::cout << "[OK] insert (1, 'Alice', 25)" << std::endl;

    Relational::Tuple row2 = { 2, std::string("Bob"), 30 };
    CHECK(engine.insert("test_relational_engine", row2), "insert row2 failed");
    std::cout << "[OK] insert (2, 'Bob', 30)" << std::endl;

    auto rows = engine.scan("test_relational_engine");
    CHECK(rows.size() == 2, "scan should return 2 rows (got " + std::to_string(rows.size()) + ")");
    std::cout << "[OK] scan returned " << rows.size() << " rows" << std::endl;

    bool found1 = false, found2 = false;
    for (const auto& row : rows) {
        CHECK(row.size() == 3, "row size");
        int id = std::get<int>(row[0]);
        std::string name = std::get<std::string>(row[1]);
        int age = std::get<int>(row[2]);
        if (id == 1) {
            CHECK(name == "Alice" && age == 25, "row1 content");
            found1 = true;
        } else if (id == 2) {
            CHECK(name == "Bob" && age == 30, "row2 content");
            found2 = true;
        }
    }
    CHECK(found1 && found2, "both rows must be found");
    std::cout << "[OK] row content verified" << std::endl;

    CHECK(!engine.has_table("nonexistent"), "has_table(nonexistent) should be false");
    CHECK(engine.get_schema("nonexistent") == nullptr, "get_schema(nonexistent) should be null");
    auto empty_scan = engine.scan("nonexistent");
    CHECK(empty_scan.empty(), "scan(nonexistent) should be empty");
    std::cout << "[OK] nonexistent table handled" << std::endl;

    CHECK(engine.drop_table("test_relational_engine"), "drop_table failed");
    std::cout << "[OK] drop_table" << std::endl;

    std::cout << "\n=== Relational Storage Engine Test PASSED ===" << std::endl;
    return 0;
}
