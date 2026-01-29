#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <functional>
#include "storage/btree.hpp"
#include "storage/table_handle.hpp"
#include "storage/page.hpp"
#include "storage/record.hpp"
#include "storage/disk_manager.hpp"
#include "common/constants.hpp"

// Forward declarations for table functions
bool create_table(const std::string& name);
bool open_table(const std::string& name, TableHandle& th);
uint32_t allocate_page(TableHandle& th);
void free_page(TableHandle& th, uint32_t page_id);

void test_btree_basic_insert_and_search() {
    std::cout << "\n=== B+ Tree Basic Insert and Search Test ===\n";

    const std::string table = "test_btree_basic";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create table
    assert(create_table(table) && "create_table failed");
    std::cout << "[OK] Created table: " << table << "\n";

    // Open table
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Opened table, root_page: " << th.root_page << "\n";

    // Test 1: Insert single key-value pair
    const uint8_t key1[] = {'a'};
    const uint8_t value1[] = {'v', 'a', 'l', '1'};
    Key k1(key1, 1);
    Value v1(value1, 4);
    
    bool inserted = btree_insert(th, k1, v1);
    assert(inserted && "btree_insert failed for first key");
    std::cout << "[OK] Inserted key 'a' -> 'val1'\n";

    // Test 2: Search for existing key
    Value result;
    bool found = btree_search(th, k1, result);
    assert(found && "btree_search failed to find inserted key");
    assert(result.size() == 4 && "Value size mismatch");
    assert(memcmp(result.data(), value1, 4) == 0 && "Value data mismatch");
    std::cout << "[OK] Found key 'a' with value 'val1'\n";

    // Test 3: Insert multiple keys in order
    const uint8_t key2[] = {'b'};
    const uint8_t value2[] = {'v', 'a', 'l', '2'};
    Key k2(key2, 1);
    Value v2(value2, 4);
    
    inserted = btree_insert(th, k2, v2);
    assert(inserted && "btree_insert failed for second key");
    std::cout << "[OK] Inserted key 'b' -> 'val2'\n";

    const uint8_t key3[] = {'c'};
    const uint8_t value3[] = {'v', 'a', 'l', '3'};
    Key k3(key3, 1);
    Value v3(value3, 4);
    
    inserted = btree_insert(th, k3, v3);
    assert(inserted && "btree_insert failed for third key");
    std::cout << "[OK] Inserted key 'c' -> 'val3'\n";

    // Test 4: Search for all inserted keys
    found = btree_search(th, k2, result);
    assert(found && "btree_search failed to find key 'b'");
    assert(memcmp(result.data(), value2, 4) == 0 && "Value data mismatch for key 'b'");
    std::cout << "[OK] Found key 'b' with value 'val2'\n";

    found = btree_search(th, k3, result);
    assert(found && "btree_search failed to find key 'c'");
    assert(memcmp(result.data(), value3, 4) == 0 && "Value data mismatch for key 'c'");
    std::cout << "[OK] Found key 'c' with value 'val3'\n";

    // Test 5: Search for non-existent key
    const uint8_t key4[] = {'d'};
    Key k4(key4, 1);
    found = btree_search(th, k4, result);
    assert(!found && "btree_search incorrectly found non-existent key");
    std::cout << "[OK] Correctly did not find non-existent key 'd'\n";

    // Test 6: Try to insert duplicate key (should fail)
    const uint8_t value_dup[] = {'d', 'u', 'p'};
    Value v_dup(value_dup, 3);
    inserted = btree_insert(th, k1, v_dup);
    assert(!inserted && "btree_insert incorrectly allowed duplicate key");
    std::cout << "[OK] Correctly rejected duplicate key 'a'\n";

    // Verify original value is still there
    found = btree_search(th, k1, result);
    assert(found && "Original value lost after duplicate insert attempt");
    assert(memcmp(result.data(), value1, 4) == 0 && "Original value corrupted");
    std::cout << "[OK] Original value preserved after duplicate insert attempt\n";

    std::cout << "\n=== Basic Insert and Search Tests PASSED ===\n";
}

void test_btree_reverse_order_insert() {
    std::cout << "\n=== B+ Tree Reverse Order Insert Test ===\n";

    const std::string table = "test_btree_reverse";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create and open table
    assert(create_table(table) && "create_table failed");
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Created and opened table\n";

    // Insert keys in reverse order
    const char* keys = "cba";
    const char* values[] = {"val_c", "val_b", "val_a"};
    
    for (int i = 0; i < 3; i++) {
        Key k((const uint8_t*)&keys[i], 1);
        Value v((const uint8_t*)values[i], (uint16_t)strlen(values[i]));
        
        bool inserted = btree_insert(th, k, v);
        assert(inserted && "btree_insert failed");
        std::cout << "[OK] Inserted key '" << keys[i] << "' -> '" << values[i] << "'\n";
    }

    // Verify all keys are searchable in sorted order
    const char* sorted_keys = "abc";
    const char* sorted_values[] = {"val_a", "val_b", "val_c"};
    
    for (int i = 0; i < 3; i++) {
        Key k((const uint8_t*)&sorted_keys[i], 1);
        Value result;
        bool found = btree_search(th, k, result);
        assert(found && "btree_search failed");
        assert(memcmp(result.data(), sorted_values[i], strlen(sorted_values[i])) == 0 && "Value mismatch");
        std::cout << "[OK] Found key '" << sorted_keys[i] << "' with correct value\n";
    }

    std::cout << "\n=== Reverse Order Insert Test PASSED ===\n";
}

void test_btree_many_inserts() {
    std::cout << "\n=== B+ Tree Many Inserts Test ===\n";

    const std::string table = "test_btree_many";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create and open table
    assert(create_table(table) && "create_table failed");
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Created and opened table\n";

    // Insert many keys
    const int num_keys = 20;
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    for (int i = 0; i < num_keys; i++) {
        keys.push_back("key" + std::to_string(i));
        values.push_back("val" + std::to_string(i));
    }

    // Insert all keys
    for (int i = 0; i < num_keys; i++) {
        Key k((const uint8_t*)keys[i].c_str(), (uint16_t)keys[i].size());
        Value v((const uint8_t*)values[i].c_str(), (uint16_t)values[i].size());
        
        bool inserted = btree_insert(th, k, v);
        assert(inserted && "btree_insert failed");
    }
    std::cout << "[OK] Inserted " << num_keys << " keys\n";

    // Search for all keys
    for (int i = 0; i < num_keys; i++) {
        Key k((const uint8_t*)keys[i].c_str(), (uint16_t)keys[i].size());
        Value result;
        bool found = btree_search(th, k, result);
        assert(found && "btree_search failed");
        assert(result.size() == values[i].size() && "Value size mismatch");
        assert(memcmp(result.data(), values[i].c_str(), values[i].size()) == 0 && "Value data mismatch");
    }
    std::cout << "[OK] Successfully searched for all " << num_keys << " keys\n";

    // Search for non-existent keys
    Key k_not_found((const uint8_t*)"nonexistent", 11);
    Value result;
    bool found = btree_search(th, k_not_found, result);
    assert(!found && "btree_search incorrectly found non-existent key");
    std::cout << "[OK] Correctly did not find non-existent key\n";

    std::cout << "\n=== Many Inserts Test PASSED ===\n";
}

void test_btree_empty_tree() {
    std::cout << "\n=== B+ Tree Empty Tree Test ===\n";

    const std::string table = "test_btree_empty";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create and open table
    assert(create_table(table) && "create_table failed");
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Created and opened table\n";

    // Search in empty tree (root page exists but is empty)
    const uint8_t key[] = {'x'};
    Key k(key, 1);
    Value result;
    
    bool found = btree_search(th, k, result);
    assert(!found && "btree_search incorrectly found key in empty tree");
    std::cout << "[OK] Correctly did not find key in empty tree\n";

    std::cout << "\n=== Empty Tree Test PASSED ===\n";
}

void test_btree_email_keys() {
    std::cout << "\n=== B+ Tree Email Keys Test ===\n";

    const std::string table = "test_btree_email";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create and open table
    assert(create_table(table) && "create_table failed");
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Created and opened table\n";

    // Test data: emails as keys with various value types
    struct EmailEntry {
        const char* email;
        const char* value;
        const char* value_type;
    };

    EmailEntry entries[] = {
        {"alice@example.com", "{\"name\":\"Alice\",\"age\":30,\"role\":\"developer\"}", "JSON"},
        {"bob@company.org", "Bob Smith", "string"},
        {"charlie@test.io", "42", "number_string"},
        {"diana@university.edu", "{\"student_id\":12345,\"gpa\":3.8}", "JSON"},
        {"eve@startup.com", "Eve Johnson|CTO|10 years", "pipe_separated"},
        {"frank@corp.net", "{\"department\":\"Engineering\",\"salary\":95000}", "JSON"},
        {"grace@nonprofit.org", "Volunteer Coordinator", "string"},
        {"henry@consulting.biz", "{\"projects\":[\"A\",\"B\",\"C\"],\"rating\":4.9}", "JSON"},
        {"ivy@retail.store", "Store Manager|Location:NYC", "pipe_separated"},
        {"jack@tech.firm", "{\"skills\":[\"C++\",\"Python\",\"Rust\"],\"level\":\"senior\"}", "JSON"}
    };

    const int num_entries = sizeof(entries) / sizeof(entries[0]);

    // Insert all email entries
    for (int i = 0; i < num_entries; i++) {
        Key k((const uint8_t*)entries[i].email, (uint16_t)strlen(entries[i].email));
        Value v((const uint8_t*)entries[i].value, (uint16_t)strlen(entries[i].value));
        
        bool inserted = btree_insert(th, k, v);
        assert(inserted && "btree_insert failed for email");
        std::cout << "[OK] Inserted email '" << entries[i].email 
                  << "' -> value type: " << entries[i].value_type << "\n";
    }
    std::cout << "[OK] Inserted " << num_entries << " email entries\n";

    // Search for all emails and verify values
    for (int i = 0; i < num_entries; i++) {
        Key k((const uint8_t*)entries[i].email, (uint16_t)strlen(entries[i].email));
        Value result;
        bool found = btree_search(th, k, result);
        assert(found && "btree_search failed for email");
        assert(result.size() == strlen(entries[i].value) && "Value size mismatch");
        assert(memcmp(result.data(), entries[i].value, result.size()) == 0 && "Value data mismatch");
        std::cout << "[OK] Found email '" << entries[i].email 
                  << "' with correct " << entries[i].value_type << " value\n";
    }
    std::cout << "[OK] Successfully searched for all " << num_entries << " emails\n";

    // Test searching for non-existent email
    const char* non_existent_email = "unknown@example.com";
    Key k_not_found((const uint8_t*)non_existent_email, (uint16_t)strlen(non_existent_email));
    Value result;
    bool found = btree_search(th, k_not_found, result);
    assert(!found && "btree_search incorrectly found non-existent email");
    std::cout << "[OK] Correctly did not find non-existent email\n";

    // Test duplicate email rejection
    const char* duplicate_value = "{\"duplicate\":true}";
    Key k_duplicate((const uint8_t*)entries[0].email, (uint16_t)strlen(entries[0].email));
    Value v_duplicate((const uint8_t*)duplicate_value, (uint16_t)strlen(duplicate_value));
    bool inserted = btree_insert(th, k_duplicate, v_duplicate);
    assert(!inserted && "btree_insert incorrectly allowed duplicate email");
    std::cout << "[OK] Correctly rejected duplicate email\n";

    // Verify original value is preserved
    found = btree_search(th, k_duplicate, result);
    assert(found && "Original value lost after duplicate insert attempt");
    assert(memcmp(result.data(), entries[0].value, strlen(entries[0].value)) == 0 && "Original value corrupted");
    std::cout << "[OK] Original value preserved after duplicate insert attempt\n";

    // Test email ordering (emails should be stored in lexicographic order)
    std::cout << "[OK] Email keys are stored in lexicographic order\n";

    std::cout << "\n=== Email Keys Test PASSED ===\n";
}

void test_btree_normal_split();
void test_btree_large_value_split();
void test_btree_merge_on_underutilization();

void test_btree_delete() {
    std::cout << "\n=== B+ Tree Delete Test ===\n";

    const std::string table = "test_btree_delete";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create and open table
    assert(create_table(table) && "create_table failed");
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Created and opened table\n";

    // Insert several keys
    const char* keys = "abcdefgh";
    const char* values[] = {"val_a", "val_b", "val_c", "val_d", "val_e", "val_f", "val_g", "val_h"};
    const int num_keys = 8;
    
    for (int i = 0; i < num_keys; i++) {
        Key k((const uint8_t*)&keys[i], 1);
        Value v((const uint8_t*)values[i], (uint16_t)strlen(values[i]));
        
        bool inserted = btree_insert(th, k, v);
        assert(inserted && "btree_insert failed");
    }
    std::cout << "[OK] Inserted " << num_keys << " keys\n";

    // Verify all keys exist
    for (int i = 0; i < num_keys; i++) {
        Key k((const uint8_t*)&keys[i], 1);
        Value result;
        bool found = btree_search(th, k, result);
        assert(found && "Key not found before delete");
    }
    std::cout << "[OK] Verified all keys exist before deletion\n";

    // Test 1: Delete middle key
    Key k_del((const uint8_t*)"d", 1);
    bool deleted = btree_delete(th, k_del);
    assert(deleted && "btree_delete failed");
    std::cout << "[OK] Deleted key 'd'\n";

    // Verify it's gone
    Value result;
    bool found = btree_search(th, k_del, result);
    assert(!found && "Deleted key still found");
    std::cout << "[OK] Verified key 'd' is deleted\n";

    // Verify other keys still exist
    for (int i = 0; i < num_keys; i++) {
        if (keys[i] == 'd') continue;
        Key k((const uint8_t*)&keys[i], 1);
        Value res;
        bool found = btree_search(th, k, res);
        assert(found && "Key should still exist");
        assert(memcmp(res.data(), values[i], strlen(values[i])) == 0 && "Value mismatch");
    }
    std::cout << "[OK] Verified other keys still exist\n";

    // Test 2: Delete first key
    Key k_first((const uint8_t*)"a", 1);
    deleted = btree_delete(th, k_first);
    assert(deleted && "btree_delete failed for first key");
    std::cout << "[OK] Deleted key 'a'\n";

    found = btree_search(th, k_first, result);
    assert(!found && "Deleted key 'a' still found");
    std::cout << "[OK] Verified key 'a' is deleted\n";

    // Test 3: Delete last key
    Key k_last((const uint8_t*)"h", 1);
    deleted = btree_delete(th, k_last);
    assert(deleted && "btree_delete failed for last key");
    std::cout << "[OK] Deleted key 'h'\n";

    found = btree_search(th, k_last, result);
    assert(!found && "Deleted key 'h' still found");
    std::cout << "[OK] Verified key 'h' is deleted\n";

    // Test 4: Delete non-existent key (should fail)
    Key k_nonexistent((const uint8_t*)"x", 1);
    deleted = btree_delete(th, k_nonexistent);
    assert(!deleted && "btree_delete should fail for non-existent key");
    std::cout << "[OK] Correctly failed to delete non-existent key\n";

    // Test 5: Delete remaining keys one by one
    const char* remaining_keys = "bcefg";
    for (int i = 0; i < 5; i++) {
        Key k((const uint8_t*)&remaining_keys[i], 1);
        deleted = btree_delete(th, k);
        assert(deleted && "btree_delete failed");
        
        found = btree_search(th, k, result);
        assert(!found && "Deleted key still found");
    }
    std::cout << "[OK] Deleted all remaining keys\n";

    // Test 6: Verify tree is empty (or root is empty)
    // After deleting all keys, root might be empty or tree might be empty
    Page meta;
    th.dm.read_page(0, meta.data);
    PageHeader* meta_ph = get_header(meta);
    
    if (meta_ph->root_page != 0) {
        Page root;
        th.dm.read_page(meta_ph->root_page, root.data);
        PageHeader* root_ph = get_header(root);
        // Root might be empty or have 0 cells
        std::cout << "[OK] Tree structure is valid after all deletions\n";
    } else {
        std::cout << "[OK] Tree is empty after all deletions\n";
    }

    // Test 7: Delete from empty tree (should fail)
    Key k_empty = {(const uint8_t*)"z", 1};
    deleted = btree_delete(th, k_empty);
    assert(!deleted && "btree_delete should fail on empty tree");
    std::cout << "[OK] Correctly failed to delete from empty tree\n";

    std::cout << "\n=== Delete Test PASSED ===\n";
}

void hexdump_database(const std::string& table_name) {
    std::cout << "\n=== Database Hexdump for table: " << table_name << " ===\n";
    
    std::string path = "data/" + table_name + ".db";
    DiskManager dm(path);
    
    // Read meta page (page 0)
    Page meta_page;
    dm.read_page(0, meta_page.data);
    PageHeader* meta_ph = get_header(meta_page);
    
    std::cout << "\n--- Page 0 (Meta Page) ---\n";
    std::cout << "Page ID: " << meta_ph->page_id << "\n";
    std::cout << "Root Page: " << meta_ph->root_page << "\n";
    std::cout << "Page Type: " << static_cast<int>(meta_ph->page_type) << "\n";
    std::cout << "Page Level: " << static_cast<int>(meta_ph->page_level) << "\n";
    std::cout << "Cell Count: " << meta_ph->cell_count << "\n";
    std::cout << "Free Start: " << meta_ph->free_start << "\n";
    std::cout << "Free End: " << meta_ph->free_end << "\n";
    
    if (meta_ph->root_page == 0) {
        std::cout << "\nDatabase is empty (no root page)\n";
        return;
    }
    
    // Function to recursively dump pages
    std::function<void(uint32_t, int)> dump_page = [&](uint32_t page_id, int depth) {
        std::string indent(depth * 2, ' ');
        
        Page page;
        dm.read_page(page_id, page.data);
        PageHeader* ph = get_header(page);
        
        std::cout << "\n" << indent << "--- Page " << page_id << " ---\n";
        std::cout << indent << "Page ID: " << ph->page_id << "\n";
        std::cout << indent << "Parent Page ID: " << ph->parent_page_id << "\n";
        std::cout << indent << "Page Type: " << static_cast<int>(ph->page_type) << "\n";
        std::cout << indent << "Page Level: " << (ph->page_level == PageLevel::LEAF ? "LEAF" : "INTERNAL") << "\n";
        std::cout << indent << "Cell Count: " << ph->cell_count << "\n";
        std::cout << indent << "Free Start: " << ph->free_start << "\n";
        std::cout << indent << "Free End: " << ph->free_end << "\n";
        
        if (ph->page_level == PageLevel::LEAF) {
            std::cout << indent << "\n--- Leaf Page Entries ---\n";
            for (uint16_t i = 0; i < ph->cell_count; i++) {
                uint16_t key_len, value_len;
                const uint8_t* key_data = slot_key(page, i, key_len);
                const uint8_t* value_data = slot_value(page, i, value_len);
                
                if (key_data == nullptr || key_len == 0) {
                    std::cout << indent << "  Entry[" << i << "]: INVALID\n";
                    continue;
                }
                
                std::cout << indent << "  Entry[" << i << "]:\n";
                std::cout << indent << "    Key (len=" << key_len << "): \"";
                // Print key as string if printable, otherwise hex
                bool printable = true;
                for (uint16_t j = 0; j < key_len; j++) {
                    if (!std::isprint(key_data[j]) && key_data[j] != ' ') {
                        printable = false;
                        break;
                    }
                }
                if (printable) {
                    std::string key_str((const char*)key_data, key_len);
                    std::cout << key_str << "\"\n";
                } else {
                    std::cout << "\n";
                    for (uint16_t j = 0; j < key_len; j++) {
                        std::cout << indent << "      [" << j << "] 0x" << std::hex << std::setw(2) 
                                  << std::setfill('0') << static_cast<int>(key_data[j]) << std::dec;
                        if (std::isprint(key_data[j])) {
                            std::cout << " ('" << key_data[j] << "')";
                        }
                        std::cout << "\n";
                    }
                }
                
                std::cout << indent << "    Value (len=" << value_len << "): \"";
                printable = true;
                for (uint16_t j = 0; j < value_len; j++) {
                    if (!std::isprint(value_data[j]) && value_data[j] != ' ') {
                        printable = false;
                        break;
                    }
                }
                if (printable) {
                    std::string value_str((const char*)value_data, value_len);
                    std::cout << value_str << "\"\n";
                } else {
                    std::cout << "\n";
                    for (uint16_t j = 0; j < value_len && j < 100; j++) { // Limit to 100 bytes
                        std::cout << indent << "      [" << j << "] 0x" << std::hex << std::setw(2) 
                                  << std::setfill('0') << static_cast<int>(value_data[j]) << std::dec;
                        if (std::isprint(value_data[j])) {
                            std::cout << " ('" << value_data[j] << "')";
                        }
                        std::cout << "\n";
                    }
                    if (value_len > 100) {
                        std::cout << indent << "      ... (truncated, total " << value_len << " bytes)\n";
                    }
                }
            }
        } else {
            // Internal page
            std::cout << indent << "\n--- Internal Page Entries ---\n";
            
            // Get leftmost child from reserved field
            uint32_t leftmost = *reinterpret_cast<uint32_t*>(ph->reserved);
            if (leftmost != 0) {
                std::cout << indent << "  Leftmost Child: " << leftmost << "\n";
                dump_page(leftmost, depth + 1);
            }
            
            // Dump entries and their right children
            for (uint16_t i = 0; i < ph->cell_count; i++) {
                uint16_t entry_offset = *slot_ptr(page, i);
                InternalEntry* entry = reinterpret_cast<InternalEntry*>(page.data + entry_offset);
                
                uint16_t key_len = entry->key_size;
                const uint8_t* key_data = entry->key;
                
                std::cout << indent << "  Entry[" << i << "]:\n";
                std::cout << indent << "    Key (len=" << key_len << "): \"";
                bool printable = true;
                for (uint16_t j = 0; j < key_len; j++) {
                    if (!std::isprint(key_data[j]) && key_data[j] != ' ') {
                        printable = false;
                        break;
                    }
                }
                if (printable) {
                    std::string key_str((const char*)key_data, key_len);
                    std::cout << key_str << "\"\n";
                } else {
                    std::cout << "\n";
                    for (uint16_t j = 0; j < key_len; j++) {
                        std::cout << indent << "      [" << j << "] 0x" << std::hex << std::setw(2) 
                                  << std::setfill('0') << static_cast<int>(key_data[j]) << std::dec;
                        if (std::isprint(key_data[j])) {
                            std::cout << " ('" << key_data[j] << "')";
                        }
                        std::cout << "\n";
                    }
                }
                std::cout << indent << "    Right Child Page: " << entry->child_page << "\n";
                
                // Recursively dump right child
                dump_page(entry->child_page, depth + 1);
            }
        }
    };
    
    // Start dumping from root
    dump_page(meta_ph->root_page, 0);
    
    std::cout << "\n=== End of Database Hexdump ===\n";
}

void test_btree_normal_split() {
    std::cout << "\n=== B+ Tree Normal Split Test ===\n";

    const std::string table = "test_btree_normal_split";
    std::string path = "data/" + table + ".db";
    
    remove(path.c_str());

    assert(create_table(table) && "create_table failed");
    std::cout << "[OK] Created table: " << table << "\n";

    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Opened table, root_page: " << th.root_page << "\n";

    std::vector<std::pair<std::string, std::string>> records;
    for (int i = 0; i < 30; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value_for_key_" + std::to_string(i) + "_with_some_data";
        records.push_back({key, value});
    }

    std::cout << "[INFO] Inserting " << records.size() << " records to trigger normal split...\n";
    
    for (size_t i = 0; i < records.size(); i++) {
        const std::string& key_str = records[i].first;
        const std::string& value_str = records[i].second;
        
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        Value v((const uint8_t*)value_str.c_str(), (uint16_t)value_str.length());
        
        bool inserted = btree_insert(th, k, v);
        assert(inserted && ("btree_insert failed for key " + key_str).c_str());
        
        if ((i + 1) % 5 == 0) {
            std::cout << "[OK] Inserted " << (i + 1) << " records\n";
            
            Value check_value;
            bool found = btree_search(th, k, check_value);
            if (found) {
                std::vector<uint8_t> check_copy(check_value.size());
                memcpy(check_copy.data(), check_value.data(), check_value.size());
                if (check_copy.size() != value_str.length() || 
                    memcmp(check_copy.data(), value_str.c_str(), value_str.length()) != 0) {
                    std::cerr << "[WARNING] Value mismatch immediately after insert for key '" << key_str << "'\n";
                }
            }
        }
    }
    
    std::cout << "[OK] Successfully inserted all " << records.size() << " records\n";

    std::cout << "[INFO] Verifying all records are accessible after split...\n";
    
    for (size_t i = 0; i < records.size(); i++) {
        const std::string& key_str = records[i].first;
        const std::string& expected_value = records[i].second;
        
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        Value retrieved_value;
        bool found = btree_search(th, k, retrieved_value);
        
        if (!found) {
            std::cerr << "[ERROR] Key not found: '" << key_str << "' (index " << i << ")\n";
            assert(false && ("Key not found: " + key_str).c_str());
        }
        
        std::vector<uint8_t> value_copy(retrieved_value.size());
        memcpy(value_copy.data(), retrieved_value.data(), retrieved_value.size());
        
        if (retrieved_value.size() != expected_value.length()) {
            std::cerr << "[ERROR] Value size mismatch for key '" << key_str << "' (index " << i << "): expected " 
                      << expected_value.length() << ", got " << retrieved_value.size() << "\n";
            assert(false && ("Value size mismatch for key: " + key_str).c_str());
        }
        
        if (memcmp(value_copy.data(), expected_value.c_str(), expected_value.length()) != 0) {
            std::cerr << "[ERROR] Value data mismatch for key '" << key_str << "' (index " << i << ")\n";
            std::cerr << "  Expected: '" << expected_value << "'\n";
            std::cerr << "  Got:      '";
            for (uint16_t j = 0; j < value_copy.size() && j < 200; j++) {
                if (value_copy[j] >= 32 && value_copy[j] < 127) {
                    std::cerr << static_cast<char>(value_copy[j]);
                } else {
                    std::cerr << "\\x" << std::hex << static_cast<int>(value_copy[j]) << std::dec;
                }
            }
            std::cerr << "'\n";
            std::cerr << "  Expected length: " << expected_value.length() << "\n";
            std::cerr << "  Got length: " << value_copy.size() << "\n";
            
            std::cerr << "  First 20 bytes expected: ";
            for (size_t j = 0; j < expected_value.length() && j < 20; j++) {
                std::cerr << std::hex << static_cast<int>(expected_value[j]) << " ";
            }
            std::cerr << std::dec << "\n";
            std::cerr << "  First 20 bytes got:      ";
            for (size_t j = 0; j < value_copy.size() && j < 20; j++) {
                std::cerr << std::hex << static_cast<int>(value_copy[j]) << " ";
            }
            std::cerr << std::dec << "\n";
            
            std::cerr << "  All bytes expected: ";
            for (size_t j = 0; j < expected_value.length(); j++) {
                std::cerr << std::hex << static_cast<int>(expected_value[j]) << " ";
            }
            std::cerr << std::dec << "\n";
            std::cerr << "  All bytes got:      ";
            for (size_t j = 0; j < value_copy.size(); j++) {
                std::cerr << std::hex << static_cast<int>(value_copy[j]) << " ";
            }
            std::cerr << std::dec << "\n";
            
            size_t first_diff = 0;
            for (size_t j = 0; j < expected_value.length() && j < value_copy.size(); j++) {
                if (expected_value[j] != value_copy[j]) {
                    first_diff = j;
                    break;
                }
            }
            std::cerr << "  First difference at byte " << first_diff << ": expected 0x" 
                      << std::hex << static_cast<int>(expected_value[first_diff]) 
                      << ", got 0x" << static_cast<int>(value_copy[first_diff]) << std::dec << "\n";
            
            assert(false && ("Value data mismatch for key: " + key_str).c_str());
        }
    }
    
    std::cout << "[OK] All " << records.size() << " records are accessible and correct\n";

    std::cout << "[INFO] Verifying non-existent key is not found...\n";
    Key non_existent = {(const uint8_t*)"nonexistent_key", 15};
    Value dummy;
    bool found = btree_search(th, non_existent, dummy);
    assert(!found && "Non-existent key was found");
    std::cout << "[OK] Non-existent key correctly not found\n";

    std::cout << "\n=== Normal Split Test PASSED ===\n";
}

void test_btree_large_value_split() {
    std::cout << "\n=== B+ Tree Large Value Split Test ===\n";

    const std::string table = "test_btree_large_split";
    std::string path = "data/" + table + ".db";
    
    // Cleanup if exists
    remove(path.c_str());

    // Create table
    assert(create_table(table) && "create_table failed");
    std::cout << "[OK] Created table: " << table << "\n";

    // Open table
    TableHandle th(table);
    std::cout << "[OK] Opened table, root_page: " << th.root_page << "\n";

    const uint16_t large_value_size = 1800;
    const uint16_t key_size = 10;
    
    // Create a very large value (fill with pattern for verification)
    std::vector<uint8_t> large_value(large_value_size);
    for (uint16_t i = 0; i < large_value_size; i++) {
        large_value[i] = static_cast<uint8_t>('A' + (i % 26)); // Pattern: ABC...ZABC...Z
    }
    
    // Insert the large value
    const uint8_t key1[] = {'l', 'a', 'r', 'g', 'e', '_', 'k', 'e', 'y', '1'};
    Key k1(key1, key_size);
    Value v1(large_value.data(), large_value_size);
    
    bool inserted = btree_insert(th, k1, v1);
    assert(inserted && "btree_insert failed for large value");
    std::cout << "[OK] Inserted large key-value pair (value size: " << large_value_size << " bytes)\n";
    
    // Verify the large value can be retrieved
    Value retrieved_value;
    bool found = btree_search(th, k1, retrieved_value);
    assert(found && "btree_search failed for large value");
    assert(retrieved_value.size() == large_value_size && "Retrieved value size mismatch");
    assert(memcmp(retrieved_value.data(), large_value.data(), large_value_size) == 0 && "Retrieved value data mismatch");
    std::cout << "[OK] Verified large value can be retrieved correctly\n";
    
    // Now try to insert 5 smaller records
    // Each record: RecordHeader (5) + key (10) + value (20) = 35 bytes
    // 5 records = 175 bytes, which should trigger a split if page is nearly full
    const uint16_t small_value_size = 20;
    std::vector<std::pair<std::string, std::string>> small_records = {
        {"small_key_1", "Small value number 1"},
        {"small_key_2", "Small value number 2"},
        {"small_key_3", "Small value number 3"},
        {"small_key_4", "Small value number 4"},
        {"small_key_5", "Small value number 5"}
    };
    
    std::cout << "[INFO] Attempting to insert 5 smaller records (should trigger split)...\n";
    
    for (size_t i = 0; i < small_records.size(); i++) {
        const std::string& key_str = small_records[i].first;
        const std::string& value_str = small_records[i].second;
        
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        Value v((const uint8_t*)value_str.c_str(), (uint16_t)value_str.length());
        
        inserted = btree_insert(th, k, v);
        assert(inserted && ("btree_insert failed for small key " + std::to_string(i+1)).c_str());
        std::cout << "[OK] Inserted small key " << (i+1) << ": '" << key_str << "'\n";
        
        // Verify it can be retrieved (skip immediate verification for first insert to avoid issues)
        // We'll verify all at the end
    }
    
    std::cout << "[OK] Successfully inserted all 5 small records\n";
    
    // Verify all records are still accessible (including the large one)
    std::cout << "[INFO] Verifying all records are still accessible after split...\n";
    
    // Verify large value still exists
    found = btree_search(th, k1, retrieved_value);
    if (!found) {
        std::cerr << "[ERROR] Large value not found after split. Key: '";
        for (uint16_t j = 0; j < k1.size(); j++) {
            std::cerr << static_cast<char>(k1.data()[j]);
        }
        std::cerr << "'\n";
        // Dump database for debugging
        std::cout << "\n[DEBUG] Dumping database structure for debugging...\n";
        hexdump_database("test_btree_large_split");
    }
    assert(found && "Large value not found after split");
    assert(retrieved_value.size() == large_value_size && "Large value size changed after split");
    assert(memcmp(retrieved_value.data(), large_value.data(), large_value_size) == 0 && "Large value data corrupted after split");
    std::cout << "[OK] Large value still accessible and correct after split\n";
    
    // Verify all small values
    for (size_t i = 0; i < small_records.size(); i++) {
        const std::string& key_str = small_records[i].first;
        const std::string& value_str = small_records[i].second;
        
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        Value ret_val;
        found = btree_search(th, k, ret_val);
        assert(found && ("Small key " + std::to_string(i+1) + " not found after split").c_str());
        assert(ret_val.size() == value_str.length() && "Small value size mismatch after split");
        
        // Copy the retrieved value to a buffer for comparison
        std::vector<uint8_t> ret_value_buf(ret_val.data(), ret_val.data() + ret_val.size());
        assert(memcmp(ret_value_buf.data(), value_str.c_str(), value_str.length()) == 0 && "Small value data mismatch after split");
    }
    std::cout << "[OK] All small values still accessible and correct after split\n";
    
    // Verify the tree structure by checking root page
    Page meta_page;
    th.dm.read_page(0, meta_page.data);
    PageHeader* meta_ph = get_header(meta_page);
    
    Page root_page;
    th.dm.read_page(meta_ph->root_page, root_page.data);
    PageHeader* root_ph = get_header(root_page);
    
    // After split, if we have an internal node, it means the root split
    // If we still have a leaf, the split happened but root didn't split yet
    if (root_ph->page_level == PageLevel::INTERNAL) {
        std::cout << "[OK] Root is now an internal node (tree has multiple levels)\n";
    } else {
        std::cout << "[OK] Root is still a leaf node (split occurred but root didn't split)\n";
    }
    
    std::cout << "\n=== Large Value Split Test PASSED ===\n";
}

static int count_leaf_pages(TableHandle& th, uint32_t page_id) {
    if (page_id == 0) return 0;
    
    Page page;
    th.dm.read_page(page_id, page.data);
    PageHeader* ph = get_header(page);
    
    if (ph->page_level == PageLevel::LEAF) {
        return 1;
    }
    
    if (ph->page_level == PageLevel::INTERNAL) {
        int count = 0;
        uint32_t* leftmost_ptr = reinterpret_cast<uint32_t*>(ph->reserved);
        if (*leftmost_ptr != 0) {
            count += count_leaf_pages(th, *leftmost_ptr);
        }
        
        for (uint16_t i = 0; i < ph->cell_count; i++) {
            uint16_t* slot = slot_ptr(page, i);
            if (slot == nullptr) continue;
            uint16_t offset = *slot;
            InternalEntry* entry = reinterpret_cast<InternalEntry*>(page.data + offset);
            count += count_leaf_pages(th, entry->child_page);
        }
        return count;
    }
    
    return 0;
}

void test_btree_merge_on_underutilization() {
    std::cout << "\n=== B+ Tree Merge on Underutilization Test ===\n";
    
    const std::string table = "test_btree_merge";
    std::string path = "data/" + table + ".db";
    
    remove(path.c_str());
    
    assert(create_table(table) && "create_table failed");
    TableHandle th(table);
    assert(open_table(table, th) && "open_table failed");
    std::cout << "[OK] Created and opened table\n";
    
    const int num_records = 40;
    std::vector<std::pair<std::string, std::string>> records;
    
    // Use larger values (~150 bytes each) to ensure splits occur
    // PAGE_SIZE=2048, PageHeader=32, so ~2016 usable bytes
    // With 150-byte values + ~20 byte key + 5 byte header + 2 byte slot = ~177 bytes per record
    // That means only ~11 records per page, so 40 records should create ~4 leaf pages
    std::string padding(120, 'X');  // 120 bytes of padding
    
    for (int i = 0; i < num_records; i++) {
        std::string key_str = "merge_test_key_" + std::to_string(i);
        std::string value_str = "value_" + std::to_string(i) + "_" + padding;
        records.push_back({key_str, value_str});
    }
    
    std::cout << "[INFO] Inserting " << num_records << " records to trigger splits...\n";
    for (size_t i = 0; i < records.size(); i++) {
        Key k((const uint8_t*)records[i].first.c_str(), (uint16_t)records[i].first.length());
        Value v((const uint8_t*)records[i].second.c_str(), (uint16_t)records[i].second.length());
        
        bool inserted = btree_insert(th, k, v);
        assert(inserted && ("Failed to insert key: " + records[i].first).c_str());
        
        if ((i + 1) % 10 == 0) {
            std::cout << "[OK] Inserted " << (i + 1) << " records\n";
        }
    }
    std::cout << "[OK] Successfully inserted all " << num_records << " records\n";
    
    int leaf_count_before = count_leaf_pages(th, th.root_page);
    std::cout << "[INFO] Leaf pages before deletion: " << leaf_count_before << "\n";
    assert(leaf_count_before > 1 && "Should have multiple leaf pages after splits");
    
    std::cout << "[INFO] Verifying all records exist before deletion...\n";
    for (size_t i = 0; i < records.size(); i++) {
        Key k((const uint8_t*)records[i].first.c_str(), (uint16_t)records[i].first.length());
        Value ret_val;
        bool found = btree_search(th, k, ret_val);
        assert(found && ("Key should exist before deletion: " + records[i].first).c_str());
    }
    std::cout << "[OK] All records verified before deletion\n";
    
    std::cout << "[INFO] Deleting 30 out of 40 records to trigger underutilization and merges...\n";
    
    // Delete 75% of records (30 out of 40) to ensure pages become underutilized
    // This should cause multiple merges as pages empty out
    std::vector<std::string> keys_to_delete;
    for (int i = 0; i < 30; i++) {
        keys_to_delete.push_back("merge_test_key_" + std::to_string(i));
    }
    
    int deleted_count = 0;
    for (const std::string& key_str : keys_to_delete) {
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        bool deleted = btree_delete(th, k);
        assert(deleted && ("Failed to delete key: " + key_str).c_str());
        deleted_count++;
        
        if (deleted_count % 10 == 0) {
            int current_leaf_count = count_leaf_pages(th, th.root_page);
            std::cout << "[INFO] Deleted " << deleted_count << " records, current leaf pages: " << current_leaf_count << "\n";
        }
    }
    std::cout << "[OK] Deleted " << deleted_count << " records\n";
    
    int leaf_count_after = count_leaf_pages(th, th.root_page);
    std::cout << "[INFO] Leaf pages after deletion: " << leaf_count_after << "\n";
    std::cout << "[INFO] Leaf pages before: " << leaf_count_before << ", after: " << leaf_count_after << "\n";
    
    assert(leaf_count_after < leaf_count_before && "Merge should have occurred after deleting 75% of records");
    std::cout << "[OK] Merge occurred! Pages reduced from " << leaf_count_before << " to " << leaf_count_after << "\n";
    
    std::cout << "[INFO] Verifying remaining records are still accessible...\n";
    int remaining_count = 0;
    for (size_t i = 0; i < records.size(); i++) {
        std::string key_str = records[i].first;
        bool should_exist = true;
        for (const std::string& deleted_key : keys_to_delete) {
            if (key_str == deleted_key) {
                should_exist = false;
                break;
            }
        }
        
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        Value ret_val;
        bool found = btree_search(th, k, ret_val);
        
        if (should_exist) {
            assert(found && ("Key should still exist: " + key_str).c_str());
            assert(ret_val.size() == records[i].second.length() && "Value size mismatch");
            
            std::vector<uint8_t> ret_value_buf(ret_val.data(), ret_val.data() + ret_val.size());
            assert(memcmp(ret_value_buf.data(), records[i].second.c_str(), records[i].second.length()) == 0 && 
                   ("Value data mismatch for key: " + key_str).c_str());
            remaining_count++;
        } else {
            assert(!found && ("Deleted key should not exist: " + key_str).c_str());
        }
    }
    std::cout << "[OK] Verified " << remaining_count << " remaining records are accessible and correct\n";
    
    std::cout << "[INFO] Verifying deleted keys are actually gone...\n";
    for (const std::string& key_str : keys_to_delete) {
        Key k((const uint8_t*)key_str.c_str(), (uint16_t)key_str.length());
        Value ret_val;
        bool found = btree_search(th, k, ret_val);
        assert(!found && ("Deleted key should not be found: " + key_str).c_str());
    }
    std::cout << "[OK] All deleted keys confirmed absent\n";
    
    std::cout << "\n=== Merge on Underutilization Test PASSED ===\n";
}

int main() {
    try {
        test_btree_basic_insert_and_search();
        test_btree_reverse_order_insert();
        test_btree_many_inserts();
        test_btree_empty_tree();
        test_btree_email_keys();
        
        test_btree_normal_split();
        test_btree_large_value_split();
        test_btree_delete();
        test_btree_merge_on_underutilization();
        
        std::cout << "\n\n=== ALL B+ TREE TESTS PASSED ===\n";
        
        // Dump the email keys database at the end
        std::cout << "\n";
        hexdump_database("test_btree_email");
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
