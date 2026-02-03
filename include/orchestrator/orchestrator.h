#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <iostream>
#include <string>

class DatabaseManager;
class TransactionManager;

/**
 * Run the full query pipeline: parse -> analyse -> print.
 * Simplified version for DDL statements only (USE, CREATE).
 *
 * @param sql SQL statement string
 * @param db_mgr Database manager (root @data/); provides storage engine for current db
 * @param txn_mgr Transaction manager for serializing queries
 * @param out Stream for result output (e.g. "Database created", "Using database")
 * @param err Stream for error messages
 */
void run_query(const std::string& sql, DatabaseManager& db_mgr, TransactionManager& txn_mgr,
               std::ostream& out, std::ostream& err);

#endif // ORCHESTRATOR_H
