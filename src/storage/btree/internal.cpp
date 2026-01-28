#include <cstdint>
#include <cassert>
#include "storage/page.hpp"
#include "storage/btree.hpp"
#include "storage/table_handle.hpp"
#include "storage/record.hpp"

uint32_t internal_find_child(Page& page, const Key& key) {
    PageHeader* ph = get_header(page);

    assert(ph->page_level == PageLevel::INTERNAL);

    int left = 0;
    int right = ph->cell_count - 1;
    int pos = ph->cell_count;

    while(left <= right) {
        int mid = (right + left) / 2;
        uint16_t mid_key_len;
        const uint8_t* mid_key = slot_key(page, mid, mid_key_len);

        auto cmp = compare_keys(key.data, key.size, mid_key, mid_key_len);

        if (cmp < 0) {
            pos = mid;
            right = mid - 1;
        } else {
            left = mid + 1;
        }

    }

    if (pos == ph->cell_count) {
        InternalEntry* entry = reinterpret_cast<InternalEntry*>(page.data + *slot_ptr(page, ph->cell_count));
        return entry->child_page;
    }
    InternalEntry* entry = reinterpret_cast<InternalEntry*>(page.data + *slot_ptr(page, pos));
    return entry->child_page;
}


uint16_t write_internal_entry(Page& page, const Key& key, uint32_t child) {
    PageHeader* ph = get_header(page);
    assert(ph->page_level== PageLevel::INTERNAL);

    uint32_t offset = ph->free_start;

    InternalEntry ieheader  {
        .key_size = key.size,
        .child_page = child
    };

    memcpy(page.data + offset, &ieheader, sizeof(ieheader));
    memcpy(page.data + offset + sizeof(ieheader), key.data, key.size);

    ph->free_start += sizeof(ieheader) + key.size;
    return offset;
}

bool insert_internal_no_split(Page& page, const Key& key, uint32_t child) {
    PageHeader* ph = get_header(page);
    assert(ph->page_level== PageLevel::INTERNAL);

    uint16_t rec_size = sizeof(InternalEntry) + key.size;
    if (!can_insert(page, rec_size)) return false;

    uint16_t offset = write_internal_entry(page, key, child);

    BSearchResult sr = search_record(page, key.data, key.size);

    if (!sr.found) return false;
    insert_slot(page, sr.index, offset);
    return true;
}

SplitInternalResult split_internal_page(TableHandle& th, Page& page) {
    auto* ph = get_header(page);
    assert(ph->page_level == PageLevel::INTERNAL);

    uint32_t new_pid = allocate_page(th);

    Page new_page;
    init_page(new_page, new_pid, PageType::INDEX, PageLevel::INTERNAL);

    uint16_t total = ph->cell_count;
    uint16_t mid = total / 2;

    uint16_t sep_len;
    const uint8_t* sep_data = slot_key(page, mid, sep_len);
    Key sep { sep_data, sep_len };

    for (uint16_t i = mid + 1; i < total; i++) {
        uint16_t offset = *slot_ptr(page, i);
        auto* ieentry = reinterpret_cast<InternalEntry*>(page.data + offset);

        uint16_t size = sizeof(InternalEntry) + ieentry->key_size;

        uint8_t buf[PAGE_SIZE];
        memcpy(buf, page.data + offset, size);

        uint16_t new_off = write_raw_record(new_page, buf, size);
        insert_slot(new_page, get_header(new_page)->cell_count, new_off);
    }

    for (int i = total - 1; i >= mid; i--) {
        remove_slot(page, i);
    }

    th.dm.write_page(new_pid, new_page.data);

    return { new_pid, sep };
}

void create_new_root(TableHandle& th, uint32_t left, const Key& key, uint32_t right) {
    uint32_t new_root_id = allocate_page(th);

    Page root;
    init_page(root, new_root_id, PageType::INDEX, PageLevel::INTERNAL);

    uint32_t offset = write_internal_entry(root, key, right);
    insert_slot(root, 0, offset);

    th.root_page = new_root_id;

    Page meta;
    th.dm.read_page(0, meta.data);
    get_header(meta)->root_page = new_root_id;
    th.dm.write_page(0, meta.data);

    th.dm.write_page(new_root_id, root.data);
}

void insert_into_parent(TableHandle& th, uint32_t left, const Key& key, uint32_t right) {
    Page left_page;
    th.dm.read_page(left, left_page.data);

    auto* lh = get_header(left_page);

    if (lh->parent_page_id == 0) {
        create_new_root(th, left, key, right);
        return;
    }

    uint32_t parent_pid = lh->parent_page_id;

    Page parent;
    th.dm.read_page(parent_pid, parent.data);

    if (insert_internal_no_split(parent, key, right)) {
        th.dm.write_page(parent_pid, parent.data);
        return;
    }

    auto split = split_internal_page(th, parent);

    th.dm.write_page(parent_pid, parent.data);

    insert_into_parent(th, parent_pid, split.seperator_key, split.new_page);
}


