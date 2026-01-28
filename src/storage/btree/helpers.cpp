#include <cstdint>
#include "storage/page.hpp"
#include "storage/btree.hpp"
#include "storage/table_handle.hpp"
#include "storage/record.hpp"
#include <assert.h>

uint16_t write_raw_record(Page& page, const uint8_t* raw, uint16_t size) {
    auto* ph = get_header(page);
    uint16_t offset = ph->free_start;

    memcpy(page.data + offset, raw, size);

    ph->free_start += size;
    return offset;
}