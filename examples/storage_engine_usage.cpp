#include "storage/interface/storage_engine.hpp"
#include <iostream>
#include <vector>
#include <string>

void scan_callback(const std::vector<uint8_t>& key, const std::vector<uint8_t>& value, void* ctx) {
    std::string key_str(key.begin(), key.end());
    std::string value_str(value.begin(), value.end());
    std::cout << "Scan: key=" << key_str << ", value=" << value_str << "\n";
}

void example_usage() {
    StorageEngine se;
    
    if (!se.create_table("users")) {
        std::cerr << "Failed to create table\n";
        return;
    }
    
    TableHandle* th = se.open_table("users");
    if (th == nullptr) {
        std::cerr << "Failed to open table\n";
        return;
    }
    
    std::vector<uint8_t> key1 = {'u', 's', 'e', 'r', '1'};
    std::vector<uint8_t> value1 = {'J', 'o', 'h', 'n', ' ', 'D', 'o', 'e'};
    
    if (!se.insert_record(th, key1, value1)) {
        std::cerr << "Failed to insert record\n";
        return;
    }
    
    std::vector<uint8_t> out_value;
    if (se.get_record(th, key1, out_value)) {
        std::string value_str(out_value.begin(), out_value.end());
        std::cout << "Retrieved value: " << value_str << "\n";
    }
    
    std::vector<uint8_t> new_value = {'J', 'a', 'n', 'e', ' ', 'D', 'o', 'e'};
    if (se.update_record(th, key1, new_value)) {
        std::cout << "Updated record\n";
    }
    
    se.scan_table(th, scan_callback, nullptr);
    
    if (se.delete_record(th, key1)) {
        std::cout << "Deleted record\n";
    }
    
    se.close_table(th);
    
    if (se.drop_table("users")) {
        std::cout << "Dropped table\n";
    }
}
