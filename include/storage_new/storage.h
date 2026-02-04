#ifndef STORAGE_H
#define STORAGE_H

#include "storage_new/catalog_manager.h"
#include "../parser/statements/create.h"
#include <string>
#include <memory>

/**
 * Simplified Storage class for DDL operations.
 * Wraps CatalogManager for USE and CREATE TABLE statements.
 */
class Storage {
private:
    std::unique_ptr<CatalogManager> catalog_;
    std::string database_path;

public:
    // Constructor: initialize with database path
    explicit Storage(const std::string& db_path = "./data/");
    ~Storage();
    
    // Get catalog manager
    CatalogManager& get_catalog();
};

#endif // STORAGE_H
