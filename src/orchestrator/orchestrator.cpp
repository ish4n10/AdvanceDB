#include "orchestrator/orchestrator.h"
#include "../parser/statements.cpp"
#include "analyser/analyser.h"
#include "../parser/statements/statement.h"
#include "../parser/statements/create.h"
#include "../parser/statements/use.h"
#include "storage_new/db_manager.h"
#include "storage_new/transaction_manager.h"
#include <stdexcept>

void run_query(const std::string& sql, DatabaseManager& db_mgr, TransactionManager& txn_mgr,
               std::ostream& out, std::ostream& err) {
    txn_mgr.execute([&](Transaction& txn) {
        try {
            Parser parser(sql);
            Statement stmt = parse_statement(parser);
            std::string db_path = db_mgr.get_current_db_path();
            AnalysisResult result = analyse(stmt, db_mgr, db_path);
            
            switch (stmt.get_type()) {
                case StatementType::Create: {
                    const CreateStmt& create_stmt = stmt.as_create();
                    if (create_stmt.is_database() && result.create_database_name) {
                        out << "Database created: " << *result.create_database_name << "\n";
                    } else if (create_stmt.is_table() && result.create_table_stmt) {
                        const CreateTableStmt& table_stmt = *result.create_table_stmt;
                        out << "Table created: " << table_stmt.table_name << "\n";
                        for (const auto& col : table_stmt.columns) {
                            out << "  - " << col.name << " " << col.data_type << "\n";
                        }
                    }
                    break;
                }

                case StatementType::Use: {
                    if (result.use_database_name) {
                        out << "Using database: " << *result.use_database_name << "\n";
                    }
                    break;
                }

                default:
                    err << "Unsupported statement type\n";
                    break;
            }
        } catch (const std::exception& e) {
            err << "Error: " << e.what() << "\n";
        }
    });
}
