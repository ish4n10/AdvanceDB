#include <cstdint>
#include "storage/table_handle.hpp"
#include "storage/page.hpp"
#include <vector>
struct Key {
    const uint8_t* data;
    uint16_t size;
};

using Value = Key;

struct SplitLeafResult {
    uint32_t new_page;
    Key seperator_key;
};

using SplitInternalResult = SplitLeafResult;

#pragma pack(push, 1)
struct InternalEntry {
    uint16_t key_size;
    uint32_t child_page;
    uint8_t key[];
};
#pragma pack(pop)


//helpers 
uint16_t write_raw_record(Page& page, const uint8_t* raw, uint16_t size);

// leaf 
uint32_t find_page_leaf(TableHandle&, const Key&);
bool btree_insert_leaf_no_split(TableHandle&, const Key&, const Value& value);

// internal
uint32_t internal_find_child(Page& page, const Key& key);