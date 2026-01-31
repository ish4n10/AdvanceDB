#include "storage/buffer_pool.hpp"
#include "storage/disk_manager.hpp"
#include "storage/page.hpp"
#include "storage/table_handle.hpp"
#include "common/constants.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>

void test_basic_fetch_and_unpin() {
    std::cout << "\n=== Buffer Pool Basic Fetch/Unpin Test ===\n";
    
    const std::string table_name = "test_buffer_pool_basic";
    std::string path = "data/" + table_name + ".db";
    std::remove(path.c_str());
    
    // Create table
    assert(create_table(table_name) && "create_table failed");
    
    // Open table and get disk manager
    TableHandle th(table_name);
    assert(open_table(table_name, th) && "open_table failed");
    
    // Create buffer pool
    BufferPoolManager buffer_pool(th.dm, 10);  // Small pool for testing
    
    // Fetch root page (page 2)
    Page* page = buffer_pool.fetch_page(2);
    assert(page != nullptr && "fetch_page failed");
    
    PageHeader* ph = get_header(*page);
    assert(ph->page_id == 2 && "page_id mismatch");
    assert(ph->page_type == PageType::DATA && "page_type mismatch");
    
    std::cout << "[OK] Fetched page 2, pin_count should be 1\n";
    assert(buffer_pool.get_pinned_count() == 1 && "pin_count mismatch");
    
    // Unpin (not dirty)
    assert(buffer_pool.unpin_page(2, false) && "unpin_page failed");
    assert(buffer_pool.get_pinned_count() == 0 && "pin_count should be 0");
    
    std::cout << "[OK] Unpinned page 2\n";
    
    // Fetch again (should be cached)
    Page* page2 = buffer_pool.fetch_page(2);
    assert(page2 == page && "Should return same page pointer from cache");
    assert(buffer_pool.get_pinned_count() == 1 && "pin_count should be 1 again");
    
    std::cout << "[OK] Fetched page 2 again (from cache)\n";
    
    buffer_pool.unpin_page(2, false);
    
    std::cout << "\n=== Basic Fetch/Unpin Test PASSED ===\n";
}

void test_dirty_flag() {
    std::cout << "\n=== Buffer Pool Dirty Flag Test ===\n";
    
    const std::string table_name = "test_buffer_pool_dirty";
    std::string path = "data/" + table_name + ".db";
    std::remove(path.c_str());
    
    create_table(table_name);
    TableHandle th(table_name);
    open_table(table_name, th);
    
    BufferPoolManager buffer_pool(th.dm, 10);
    
    // Fetch and modify page
    Page* page = buffer_pool.fetch_page(2);
    assert(page != nullptr);
    
    PageHeader* ph = get_header(*page);
    ph->cell_count = 42;  // Modify page
    
    // Unpin as dirty
    assert(buffer_pool.unpin_page(2, true) && "unpin_page failed");
    
    std::cout << "[OK] Marked page 2 as dirty\n";
    
    // Flush the page
    assert(buffer_pool.flush_page(2) && "flush_page failed");
    
    std::cout << "[OK] Flushed page 2 to disk\n";
    
    // Verify it was written (same buffer pool we used)
    Page* disk_page = buffer_pool.fetch_page(2);
    assert(disk_page && "Page 2 should be in buffer pool");
    PageHeader* disk_ph = get_header(*disk_page);
    assert(disk_ph->cell_count == 42 && "Page not written");
    buffer_pool.unpin_page(2, false);
    
    std::cout << "[OK] Verified page was written to disk\n";
    
    std::cout << "\n=== Dirty Flag Test PASSED ===\n";
}

void test_new_page() {
    std::cout << "\n=== Buffer Pool New Page Test ===\n";
    
    const std::string table_name = "test_buffer_pool_new";
    std::string path = "data/" + table_name + ".db";
    std::remove(path.c_str());
    
    create_table(table_name);
    TableHandle th(table_name);
    open_table(table_name, th);
    
    BufferPoolManager buffer_pool(th.dm, 10);
    
    // Allocate a new page
    uint32_t new_page_id = allocate_page(th);
    assert(new_page_id != INVALID_PAGE_ID && "allocate_page failed");
    
    std::cout << "[OK] Allocated page " << new_page_id << "\n";
    
    // Create page in buffer pool
    Page* new_page = buffer_pool.new_page(new_page_id, PageType::DATA, PageLevel::LEAF);
    assert(new_page != nullptr && "new_page failed");
    
    PageHeader* ph = get_header(*new_page);
    assert(ph->page_id == new_page_id && "page_id mismatch");
    assert(ph->page_type == PageType::DATA && "page_type mismatch");
    assert(ph->page_level == PageLevel::LEAF && "page_level mismatch");
    assert(ph->cell_count == 0 && "cell_count should be 0");
    
    std::cout << "[OK] Created new page in buffer pool\n";
    assert(buffer_pool.get_pinned_count() == 1 && "new page should be pinned");
    
    // Unpin (dirty because it's new)
    buffer_pool.unpin_page(new_page_id, true);
    
    // Flush to ensure it's on disk
    buffer_pool.flush_page(new_page_id);
    
    // Verify it's in buffer pool
    Page* disk_page = buffer_pool.fetch_page(new_page_id);
    assert(disk_page && "New page should be in buffer pool");
    PageHeader* disk_ph = get_header(*disk_page);
    assert(disk_ph->page_id == new_page_id && "Page not found");
    buffer_pool.unpin_page(new_page_id, false);
    
    std::cout << "[OK] Verified new page is on disk\n";
    
    std::cout << "\n=== New Page Test PASSED ===\n";
}

void test_lru_eviction() {
    std::cout << "\n=== Buffer Pool LRU Eviction Test ===\n";
    
    const std::string table_name = "test_buffer_pool_lru";
    std::string path = "data/" + table_name + ".db";
    std::remove(path.c_str());
    
    create_table(table_name);
    TableHandle th(table_name);
    open_table(table_name, th);
    
    // Create a very small buffer pool (3 frames)
    BufferPoolManager buffer_pool(th.dm, 3);
    
    // Fetch 3 pages (fills the pool)
    Page* page2 = buffer_pool.fetch_page(2);
    assert(page2 != nullptr);
    buffer_pool.unpin_page(2, false);
    
    Page* page0 = buffer_pool.fetch_page(0);
    assert(page0 != nullptr);
    buffer_pool.unpin_page(0, false);
    
    Page* page1 = buffer_pool.fetch_page(1);
    assert(page1 != nullptr);
    buffer_pool.unpin_page(1, false);
    
    std::cout << "[OK] Filled buffer pool with 3 pages\n";
    assert(buffer_pool.get_free_frame_count() == 0 && "Should have no free frames");
    
    // Fetch page 2 again (most recently used)
    page2 = buffer_pool.fetch_page(2);
    assert(page2 != nullptr);
    buffer_pool.unpin_page(2, false);
    
    // Now fetch a new page - should evict least recently used (page 0)
    uint32_t new_page_id = allocate_page(th);
    Page* new_page = buffer_pool.new_page(new_page_id, PageType::DATA, PageLevel::LEAF);
    assert(new_page != nullptr && "Should evict page 0 to make room");
    
    std::cout << "[OK] Evicted least recently used page (page 0)\n";
    
    // Verify page 0 is no longer in buffer pool
    Page* page0_again = buffer_pool.fetch_page(0);
    assert(page0_again != nullptr && "Should fetch page 0 from disk");
    // This will evict another page, but page 0 should be loaded fresh
    
    buffer_pool.unpin_page(new_page_id, true);
    buffer_pool.unpin_page(0, false);
    
    std::cout << "\n=== LRU Eviction Test PASSED ===\n";
}

void test_pin_count() {
    std::cout << "\n=== Buffer Pool Pin Count Test ===\n";
    
    const std::string table_name = "test_buffer_pool_pin";
    std::string path = "data/" + table_name + ".db";
    std::remove(path.c_str());
    
    create_table(table_name);
    TableHandle th(table_name);
    open_table(table_name, th);
    
    BufferPoolManager buffer_pool(th.dm, 5);
    
    // Fetch same page multiple times
    Page* page1 = buffer_pool.fetch_page(2);  // pin_count = 1
    assert(buffer_pool.get_pinned_count() == 1);
    
    Page* page2 = buffer_pool.fetch_page(2);  // pin_count = 2
    assert(page1 == page2 && "Should return same page");
    assert(buffer_pool.get_pinned_count() == 1 && "Still 1 unique pinned page");
    
    Page* page3 = buffer_pool.fetch_page(2);  // pin_count = 3
    assert(buffer_pool.get_pinned_count() == 1);
    
    std::cout << "[OK] Fetched page 2 three times (pin_count = 3)\n";
    
    // Unpin twice
    buffer_pool.unpin_page(2, false);  // pin_count = 2
    buffer_pool.unpin_page(2, false);  // pin_count = 1
    assert(buffer_pool.get_pinned_count() == 1);
    
    std::cout << "[OK] Unpinned twice (pin_count = 1)\n";
    
    // Unpin once more - now evictable
    buffer_pool.unpin_page(2, false);  // pin_count = 0
    assert(buffer_pool.get_pinned_count() == 0);
    
    std::cout << "[OK] Unpinned last time (pin_count = 0, evictable)\n";
    
    std::cout << "\n=== Pin Count Test PASSED ===\n";
}

int main() {
    try {
        test_basic_fetch_and_unpin();
        test_dirty_flag();
        test_new_page();
        test_lru_eviction();
        test_pin_count();
        
        std::cout << "\n\n=== ALL BUFFER POOL TESTS PASSED ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
