#include "storage/interface/storage_engine.hpp"
#include "storage/table_handle.hpp"
#include "storage/btree.hpp"
#include <cstring>
#include <algorithm>
#include <cstdio>

StorageEngine::StorageEngine() = default;

StorageEngine::~StorageEngine() {
    flush_all();
    open_tables_.clear();
}

bool StorageEngine::create_table(const std::string& table_name) {
    if (open_tables_.find(table_name) != open_tables_.end()) {
        return false;
    }
    return ::create_table(table_name);
}

bool StorageEngine::drop_table(const std::string& table_name) {
    auto it = open_tables_.find(table_name);
    if (it != open_tables_.end()) {
        open_tables_.erase(it);
    }
    
    std::string path = "data/" + table_name + ".db";
    return std::remove(path.c_str()) == 0;
}

TableHandle* StorageEngine::open_table(const std::string& table_name) {
    return get_or_open_table(table_name);
}

void StorageEngine::close_table(TableHandle* handle) {
    if (handle == nullptr) {
        return;
    }
    
    for (auto it = open_tables_.begin(); it != open_tables_.end(); ++it) {
        if (it->second.get() == handle) {
            handle->dm.flush();
            open_tables_.erase(it);
            return;
        }
    }
}

TableHandle* StorageEngine::get_or_open_table(const std::string& table_name) {
    auto it = open_tables_.find(table_name);
    if (it != open_tables_.end()) {
        return it->second.get();
    }
    
    auto th = std::make_unique<TableHandle>(table_name);
    if (!::open_table(table_name, *th)) {
        return nullptr;
    }
    
    TableHandle* handle = th.get();
    open_tables_[table_name] = std::move(th);
    return handle;
}

bool StorageEngine::insert_record(TableHandle* handle, const std::vector<uint8_t>& key, const std::vector<uint8_t>& value) {
    if (handle == nullptr || key.empty() || key.size() > UINT16_MAX || value.size() > UINT16_MAX) {
        return false;
    }
    
    Key k(key.data(), static_cast<uint16_t>(key.size()));
    Value v(value.data(), static_cast<uint16_t>(value.size()));
    
    return btree_insert(*handle, k, v);
}

bool StorageEngine::get_record(TableHandle* handle, const std::vector<uint8_t>& key, std::vector<uint8_t>& out_value) {
    if (handle == nullptr || key.empty() || key.size() > UINT16_MAX) {
        return false;
    }
    
    Key k(key.data(), static_cast<uint16_t>(key.size()));
    Value v;
    
    if (!btree_search(*handle, k, v)) {
        return false;
    }
    
    out_value.assign(v.data(), v.data() + v.size());
    return true;
}

bool StorageEngine::delete_record(TableHandle* handle, const std::vector<uint8_t>& key) {
    if (handle == nullptr || key.empty() || key.size() > UINT16_MAX) {
        return false;
    }
    
    Key k(key.data(), static_cast<uint16_t>(key.size()));
    return btree_delete(*handle, k);
}

bool StorageEngine::update_record(TableHandle* handle, const std::vector<uint8_t>& key, const std::vector<uint8_t>& new_value) {
    if (handle == nullptr || key.empty() || key.size() > UINT16_MAX || new_value.size() > UINT16_MAX) {
        return false;
    }
    
    Key k(key.data(), static_cast<uint16_t>(key.size()));
    
    if (!btree_delete(*handle, k)) {
        return false;
    }
    
    Value v(new_value.data(), static_cast<uint16_t>(new_value.size()));
    return btree_insert(*handle, k, v);
}

namespace {
struct ScanContext {
    StorageEngine::ScanCallback user_callback;
    void* user_ctx;
};

void btree_scan_wrapper(const Key& k, const Value& v, void* ctx) {
    ScanContext* scan_ctx = static_cast<ScanContext*>(ctx);
    std::vector<uint8_t> key_vec(k.data(), k.data() + k.size());
    std::vector<uint8_t> value_vec(v.data(), v.data() + v.size());
    scan_ctx->user_callback(key_vec, value_vec, scan_ctx->user_ctx);
}
}

void StorageEngine::scan_table(TableHandle* handle, ScanCallback callback, void* ctx) {
    if (handle == nullptr || callback == nullptr) {
        return;
    }
    
    std::vector<uint8_t> empty;
    range_scan(handle, empty, empty, callback, ctx);
}

void StorageEngine::range_scan(TableHandle* handle, const std::vector<uint8_t>& start_key, const std::vector<uint8_t>& end_key, ScanCallback callback, void* ctx) {
    if (handle == nullptr || callback == nullptr) {
        return;
    }
    
    Key k_start, k_end;
    if (!start_key.empty() && start_key.size() <= UINT16_MAX) {
        k_start = Key(start_key.data(), static_cast<uint16_t>(start_key.size()));
    }
    if (!end_key.empty() && end_key.size() <= UINT16_MAX) {
        k_end = Key(end_key.data(), static_cast<uint16_t>(end_key.size()));
    }
    
    ScanContext scan_ctx;
    scan_ctx.user_callback = callback;
    scan_ctx.user_ctx = ctx;
    
    btree_range_scan(*handle, k_start, k_end, btree_scan_wrapper, &scan_ctx);
}

void StorageEngine::flush_all() {
    for (auto& [name, handle] : open_tables_) {
        if (handle) {
            handle->dm.flush();
        }
    }
}
