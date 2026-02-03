#include <iostream>
#include <string>
#include <algorithm>
#include "storage_new/db_manager.h"
#include "storage_new/transaction_manager.h"
#include "orchestrator/orchestrator.h"

int main(int argc, char* argv[]) {
    DatabaseManager db_mgr("@data/");
    TransactionManager txn_mgr;
    
    if (argc > 1) {
        // Command line mode: execute each argument as SQL
        for (int i = 1; i < argc; ++i) {
            std::string sql = argv[i];
            run_query(sql, db_mgr, txn_mgr, std::cout, std::cerr);
        }
    } else {
        // Interactive mode: read from stdin
        std::cout << "Type SQL statements ending with ; (or 'exit;' / 'quit;' to exit)\n\n";
        
        std::string line;
        std::string sql;
        
        while (std::getline(std::cin, line)) {
            sql += line + " ";
            
            // Check if line ends with semicolon
            if (line.find(';') != std::string::npos) {
                // Trim and check for exit
                std::string sql_trimmed = sql;
                sql_trimmed.erase(0, sql_trimmed.find_first_not_of(" \t\n\r"));
                sql_trimmed.erase(sql_trimmed.find_last_not_of(" \t\n\r") + 1);
                
                // Convert to lowercase for comparison
                std::string sql_lower = sql_trimmed;
                std::transform(sql_lower.begin(), sql_lower.end(), sql_lower.begin(), ::tolower);
                
                if (sql_lower == "exit;" || sql_lower == "quit;") {
                    std::cout << "Ok\n";
                    break;
                }
                
                // Execute SQL
                run_query(sql_trimmed, db_mgr, txn_mgr, std::cout, std::cerr);
                sql.clear();
            }
        }
    }
    
    return 0;
}
