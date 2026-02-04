#include "storage_new/storage.h"
#include "storage_new/catalog_manager.h"
#include <filesystem>

Storage::Storage(const std::string& db_path) : database_path(db_path) {
    std::filesystem::create_directories(db_path);
    catalog_ = std::make_unique<CatalogManager>();
}

Storage::~Storage() = default;

CatalogManager& Storage::get_catalog() {
    return *catalog_;
}
