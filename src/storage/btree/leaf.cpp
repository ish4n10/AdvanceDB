#include <cstdint>
#include "storage/page.hpp"
#include "storage/btree.hpp"
#include "storage/table_handle.hpp"
#include "storage/record.hpp"
#include <cassert>
#include <vector>
#include <cstring>

uint32_t find_leaf_page(TableHandle& th, const Key& key, Page& out_page) {
    uint32_t page_id = th.root_page;
    int depth = 0;
    
    while(1) {
        th.dm.read_page(page_id, out_page.data);
        auto* ph = get_header(out_page);
        
        if (ph->page_level == PageLevel::LEAF) {
            return page_id;
        }
        
        uint32_t next_page_id = internal_find_child(out_page, key);
        if (next_page_id == 0 || next_page_id >= 1000000) {
            return UINT32_MAX;
        }
        
        page_id = next_page_id;
        depth++;
        if (depth > 100) {
            return UINT32_MAX;
        }
    }
}

uint32_t find_leftmost_leaf_page(TableHandle& th, Page& out_page) {
    if (th.root_page == 0) {
        return UINT32_MAX;
    }
    uint32_t page_id = th.root_page;
    int depth = 0;
    while (true) {
        th.dm.read_page(page_id, out_page.data);
        PageHeader* ph = get_header(out_page);
        if (ph->page_level == PageLevel::LEAF) {
            return page_id;
        }
        if (ph->page_level != PageLevel::INTERNAL) {
            return UINT32_MAX;
        }
        uint32_t* leftmost_ptr = reinterpret_cast<uint32_t*>(ph->reserved);
        page_id = *leftmost_ptr;
        if (page_id == 0) {
            return UINT32_MAX;
        }
        depth++;
        if (depth > 100) {
            return UINT32_MAX;
        }
    }
}

bool btree_insert_leaf_no_split(TableHandle& th, uint32_t page_id, Page& page, const Key& key, const Value& value) {
    uint16_t rec_size = record_size(key.size(), value.size());
    if (!can_insert(page, rec_size)) {
        return false;
    }
    page_insert(page, key.data(), key.size(), value.data(), value.size());
    th.dm.write_page(page_id, page.data);
    return true;
}

SplitLeafResult split_leaf_page(TableHandle& th, Page& page) {
    PageHeader* ph = get_header(page);
    assert(ph->page_level == PageLevel::LEAF);

    uint16_t total = ph->cell_count;
    if (total == 0) {
        assert(false && "Cannot split empty page");
        return {0, Key()};
    }

    uint16_t split_idx = total / 2;
    if (split_idx == 0) {
        split_idx = 1;
    }

    uint32_t left_page_id = ph->page_id;
    uint32_t saved_parent_id = ph->parent_page_id;
    uint32_t old_next_page_id = ph->next_page_id;

    struct Record {
        std::vector<uint8_t> key;
        std::vector<uint8_t> value;
    };
    std::vector<Record> all_records;
    
    for (uint16_t i = 0; i < total; i++) {
        uint16_t key_len = 0;
        const uint8_t* key_data = slot_key(page, i, key_len);
        uint16_t value_len = 0;
        const uint8_t* value_data = slot_value(page, i, value_len);
        
        if (key_data == nullptr || value_data == nullptr) {
            assert(false && "Failed to read record");
            return {0, Key()};
        }
        
        Record rec;
        rec.key.assign(key_data, key_data + key_len);
        rec.value.assign(value_data, value_data + value_len);
        all_records.push_back(std::move(rec));
    }

    init_page(page, left_page_id, PageType::DATA, PageLevel::LEAF);
    ph = get_header(page);
    ph->parent_page_id = saved_parent_id;

    uint32_t new_page_id = allocate_page(th);
    Page new_page;
    init_page(new_page, new_page_id, PageType::DATA, PageLevel::LEAF);
    PageHeader* new_ph = get_header(new_page);
    new_ph->parent_page_id = saved_parent_id;

    std::vector<uint16_t> left_offsets;
    for (uint16_t i = 0; i < split_idx; i++) {
        const auto& rec = all_records[i];
        uint16_t offset = write_record(page, rec.key.data(), rec.key.size(), rec.value.data(), rec.value.size());
        left_offsets.push_back(offset);
    }
    ph = get_header(page);
    ph->free_end = PAGE_SIZE - static_cast<uint16_t>(left_offsets.size() * sizeof(uint16_t));
    for (uint16_t i = 0; i < left_offsets.size(); i++) {
        uint16_t* slot = reinterpret_cast<uint16_t*>(page.data + ph->free_end + i * sizeof(uint16_t));
        *slot = left_offsets[i];
    }
    ph->cell_count = static_cast<uint16_t>(left_offsets.size());

    std::vector<uint16_t> right_offsets;
    for (uint16_t i = split_idx; i < total; i++) {
        const auto& rec = all_records[i];
        uint16_t offset = write_record(new_page, rec.key.data(), rec.key.size(), rec.value.data(), rec.value.size());
        right_offsets.push_back(offset);
    }
    new_ph = get_header(new_page);
    new_ph->free_end = PAGE_SIZE - static_cast<uint16_t>(right_offsets.size() * sizeof(uint16_t));
    for (uint16_t i = 0; i < right_offsets.size(); i++) {
        uint16_t* slot = reinterpret_cast<uint16_t*>(new_page.data + new_ph->free_end + i * sizeof(uint16_t));
        *slot = right_offsets[i];
    }
    new_ph->cell_count = static_cast<uint16_t>(right_offsets.size());

    ph = get_header(page);
    new_ph = get_header(new_page);

    if (ph->cell_count == 0 || new_ph->cell_count == 0) {
        assert(false && "Page is empty after split");
        return {0, Key()};
    }

    uint16_t sep_len;
    const uint8_t* sep_data = slot_key(new_page, 0, sep_len);
    if (sep_data == nullptr || sep_len == 0) {
        assert(false && "Failed to get separator key from new page");
        return {0, Key()};
    }
    
    if (sep_len > 256) {
        assert(false && "Separator key too large");
        return {0, Key()};
    }
    
    Key sep_key;
    sep_key.assign(sep_data, sep_len);

    ph->next_page_id = new_page_id;
    new_ph->prev_page_id = left_page_id;
    new_ph->next_page_id = old_next_page_id;
    if (old_next_page_id != 0) {
        Page old_next_page;
        th.dm.read_page(old_next_page_id, old_next_page.data);
        get_header(old_next_page)->prev_page_id = new_page_id;
        th.dm.write_page(old_next_page_id, old_next_page.data);
    }

    th.dm.write_page(left_page_id, page.data);
    th.dm.write_page(new_page_id, new_page.data);

    return {
        new_page_id,
        sep_key,
        page,
        new_page
    };
}
