#ifndef ANALYSER_H
#define ANALYSER_H

#include "../parser/statements/create.h"
#include "storage_new/catalog_manager.h"
#include <optional>
#include <string>

class Statement;
class DatabaseManager;

/**
 * Result of analysis: data the orchestrator needs for output/execution.
 * Analyser performs all catalog/db side effects (CREATE, USE)
 * and returns only the data needed to print.
 */
struct AnalysisResult {
    // DDL performed by analyser (orchestrator only prints message)
    std::optional<std::string> create_database_name;
    std::optional<CreateTableStmt> create_table_stmt;
    std::optional<std::string> use_database_name;
};

/**
 * Analyse statement and perform catalog/db side effects for DDL.
 * Catalog is obtained from db_mgr.get_storage_engine()->get_catalog() when needed.
 *
 * @param stmt Parsed statement
 * @param db_mgr Database manager (USE, CREATE DATABASE, get_storage_engine, get_current_db_path)
 * @param db_path Current database path (from DatabaseManager::get_current_db_path())
 * @return AnalysisResult with DDL names for printing
 */
AnalysisResult analyse(const Statement& stmt, DatabaseManager& db_mgr, const std::string& db_path);

#endif // ANALYSER_H
