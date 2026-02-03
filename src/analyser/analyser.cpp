#include "analyser/analyser.h"
#include "../parser/statements/statement.h"
#include "../parser/statements/create.h"
#include "../parser/statements/use.h"
#include "storage_new/db_manager.h"
#include "storage_new/catalog_manager.h"
#include "storage_new/storage.h"
#include "storage_new/page/page.h"
#include <stdexcept>
#include <filesystem>
#include <algorithm>

AnalysisResult analyse(const Statement& stmt, DatabaseManager& db_mgr, const std::string& db_path) {
    AnalysisResult result;

    switch (stmt.get_type()) {
        case StatementType::Use: {
            const UseStmt& use_stmt = stmt.as_use();
            if (!db_mgr.database_exists(use_stmt.database_name)) {
                throw std::runtime_error("Database '" + use_stmt.database_name + "' does not exist");
            }
            db_mgr.use_db(use_stmt.database_name);
            result.use_database_name = use_stmt.database_name;
            break;
        }

        case StatementType::Create: {
            const CreateStmt& create_stmt = stmt.as_create();
            if (create_stmt.is_database()) {
                const CreateDatabaseStmt& db_stmt = create_stmt.as_database();
                if (db_mgr.database_exists(db_stmt.database_name)) {
                    throw std::runtime_error("Database '" + db_stmt.database_name + "' already exists");
                }
                db_mgr.create_db(db_stmt.database_name);
                result.create_database_name = db_stmt.database_name;
            } else if (create_stmt.is_table()) {
                const CreateTableStmt& table_stmt = create_stmt.as_table();
                if (db_path.empty()) {
                    throw std::runtime_error("No database selected. Use USE <db>; first.");
                }
                Storage* eng = db_mgr.get_storage_engine();
                if (!eng) {
                    throw std::runtime_error("No database selected. Use USE <db>; first.");
                }
                CatalogManager& catalog = eng->get_catalog();
                std::string table_path = db_path + table_stmt.table_name + ".ibd";
                if (std::filesystem::exists(table_path)) {
                    throw std::runtime_error("Table '" + table_stmt.table_name + "' already exists");
                }
                if (table_stmt.columns.empty()) {
                    throw std::runtime_error("CREATE TABLE must have at least one column");
                }
                for (size_t i = 0; i < table_stmt.columns.size(); ++i) {
                    for (size_t j = i + 1; j < table_stmt.columns.size(); ++j) {
                        if (table_stmt.columns[i].name == table_stmt.columns[j].name) {
                            throw std::runtime_error("Duplicate column name '" + table_stmt.columns[i].name + "' in CREATE TABLE");
                        }
                    }
                }
                // IndexPolicy: only one PRIMARY KEY, no composite
                int pk_count = 0;
                for (const auto& col : table_stmt.columns) {
                    if (col.is_primary_key) pk_count++;
                }
                if (pk_count > 1) {
                    throw std::runtime_error("At most one PRIMARY KEY column is allowed. Composite primary keys are not supported.");
                }
                // pk_count == 0 is now allowed; row_id will be used as key
                catalog.create_table_meta(db_path, table_stmt.table_name, table_stmt);
                result.create_table_stmt = table_stmt;
            }
            break;
        }

        default:
            // Other statement types not supported in simplified version
            throw std::runtime_error("Unsupported statement type in analyser");
    }

    return result;
}
