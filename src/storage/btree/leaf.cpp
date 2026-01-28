#include <cstdint>
#include "storage/page.hpp"
#include "storage/btree.hpp"
#include "storage/table_handle.hpp"
#include "storage/record.hpp"
#include <assert.h>

uint32_t find_leaf_page(TableHandle& th, const Key& key) {
    uint32_t page_id = th.root_page;
    
    while(1) {
        Page page;
        th.dm.read_page(page_id, page.data);
        
        auto* ph = get_header(page);
        
        if (ph->page_level == PageLevel::LEAF) {
            return page_id;
        }
        page_id = internal_find_child(page, key);
    }

    return UINT32_MAX; // for any error
}


bool btree_insert_leaf_no_split(TableHandle& th, const Key& key, const Value& value) {
    uint32_t page_id = find_leaf_page(th, key);
    
    Page page;
    th.dm.read_page(page_id, page.data);

    if (!can_insert(page, record_size(key.size,  value.size))) {
        return false;
    }

    page_insert(page, key.data, key.size, value.data, value.size);

    th.dm.write_page(page_id, page.data);
    return true;
}


SplitLeafResult split_leaf_page(TableHandle& th, Page& page) {
    PageHeader* ph = get_header(page);
    assert(ph->page_level == PageLevel::LEAF);

    uint32_t new_page_id = allocate_page(th);

    Page new_page;
    init_page(new_page, new_page_id, PageType::DATA, PageLevel::LEAF);

    uint16_t total = ph->cell_count;
    uint32_t split_index = total / 2;

    for(uint16_t i = split_index; i < total; i++) {
        uint16_t offset = *slot_ptr(page, i);

        auto* rh = reinterpret_cast<RecordHeader*>(page.data + offset);
        auto rec_size = record_size(rh->key_size, rh->value_size);

        uint8_t buffer[PAGE_SIZE];
        memcpy(buffer, page.data + offset, rec_size);

        uint16_t new_offset = write_raw_record(new_page, buffer, rec_size);
        auto* new_ph = get_header(new_page);
        insert_slot(new_page, new_ph->cell_count, new_offset);
    }

    for(int i = total - 1; i >= split_index; i--) {
        remove_slot(page, i);
    }

    uint16_t sep_len;
    const uint8_t* sep_data = slot_key(new_page, 0, sep_len);

    Key sep_key {sep_data, sep_len};

    th.dm.write_page(new_page_id, new_page.data);

    return {
        new_page_id,
        sep_key
    };
}