# Low-Level Memory Explanation: Why `free_end` Update Order Matters

## Memory Layout Invariant

In a slotted-page layout:
- **Records grow upward**: from `free_start` (low addresses) toward higher addresses
- **Slot directory grows downward**: from `free_end` (high addresses) toward lower addresses
- **Invariant**: `free_start <= free_end` (they must not cross)

For a page of size `PAGE_SIZE = 8192` bytes:
- Address range: `[0, 8191]`
- `free_start` typically starts at `32` (after PageHeader)
- `free_end` starts at `8192` (end of page)

## Slot Address Calculation

Your `slot_ptr(page, i)` computes:
```cpp
slot_address = page.data + header->free_end + (i * sizeof(uint16_t))
```

**Critical assumption**: This formula assumes slots are stored **contiguously starting from `free_end`**.

## Why Order Matters: Concrete Example

### Scenario: Inserting slot[1] when slot[0] already exists

**Initial state** (after inserting slot[0] = 12):
```
free_end = 8190  (was 8192, decremented by 2)
cell_count = 1
Memory layout:
  [0-31]:     PageHeader
  [32-8189]:  Records (free space)
  [8190]:     Slot[0] = 12  ← stored at offset 8190
  [8191]:     (unused, beyond page)
```

**WRONG APPROACH: Shift before updating `free_end`**

```cpp
// Current state: free_end = 8190, cell_count = 1
// We want to insert slot[1] = 13

// Step 1: Shift slot[0] to slot[1]
slot_ptr(page, 1) = slot_ptr(page, 0)
```

**What happens at byte level:**

1. **Compute `slot_ptr(page, 1)`** (destination):
   - `offset = free_end + (1 * 2) = 8190 + 2 = 8192`
   - **Problem**: Offset 8192 is **beyond the page boundary** (valid range is [0, 8191])
   - This writes to **invalid memory** (possibly corrupting adjacent page or causing segfault)

2. **Compute `slot_ptr(page, 0)`** (source):
   - `offset = free_end + (0 * 2) = 8190 + 0 = 8190` ✓ (correct current position)

3. **Copy operation**:
   - `*8192 = *8190` → **Writes to invalid memory at 8192**
   - The value 12 is copied to an invalid location

4. **Then update `free_end`**:
   - `free_end = 8190 - 2 = 8188`
   - Now `slot_ptr(page, 1)` would compute to `8188 + 2 = 8190`, but we already wrote garbage at 8192!

**Result**: Memory corruption, wrong slot values, or crash.

---

## Why This Happens: The Invariant Violation

The slot address formula assumes:
```
slot[i] is located at: free_end + (i * sizeof(uint16_t))
```

But this is only true **after** `free_end` has been updated to reflect the new slot count.

**Before update**: `free_end` still reflects the **old** slot count
- `free_end = 8190` (reflects 1 slot)
- Computing `slot_ptr(page, 1)` gives `8190 + 2 = 8192` (wrong!)

**After update**: `free_end` reflects the **new** slot count
- `free_end = 8188` (reflects 2 slots)
- Computing `slot_ptr(page, 1)` gives `8188 + 2 = 8190` (correct!)

---

## Correct Approach (a): Update `free_end` First

```cpp
void insert_slot(Page& page, uint16_t index, uint16_t record_offset) {
    PageHeader* header = get_header(page);
    
    // STEP 1: Update free_end FIRST (before any slot_ptr calls)
    header->free_end -= sizeof(uint16_t);  // free_end = 8188
    
    // STEP 2: Now slot_ptr uses the updated free_end
    // Shift existing slots
    for(int i = header->cell_count; i > index; --i) {
        *slot_ptr(page, i) = *slot_ptr(page, i - 1);
        // slot_ptr(page, i)   → free_end + i*2 = 8188 + 2 = 8190 ✓
        // slot_ptr(page, i-1) → free_end + (i-1)*2 = 8188 + 0 = 8188 ✓
    }
    
    // STEP 3: Write new slot
    *slot_ptr(page, index) = record_offset;
    // slot_ptr(page, index) → free_end + index*2 = 8188 + 2 = 8190 ✓
    
    header->cell_count += 1;
}
```

**Memory layout after insertion**:
```
free_end = 8188
cell_count = 2
Memory:
  [8188-8189]: Slot[0] = 12  ← moved here
  [8190-8191]: Slot[1] = 13  ← new slot here
```

**Why this works**: All `slot_ptr()` calls use the **same** `free_end` value (8188), so addresses are consistent throughout the operation.

---

## Correct Approach (b): Freeze Slot Base Address

```cpp
void insert_slot(Page& page, uint16_t index, uint16_t record_offset) {
    PageHeader* header = get_header(page);
    
    // STEP 1: Compute slot base address ONCE (freeze it)
    uint16_t current_free_end = header->free_end;  // e.g., 8190
    uint8_t* slot_base = page.data + current_free_end;  // Base = 8190
    
    // STEP 2: Shift slots using frozen base
    for(int i = header->cell_count; i > index; --i) {
        uint16_t* dest = reinterpret_cast<uint16_t*>(slot_base + i * sizeof(uint16_t));
        uint16_t* src = reinterpret_cast<uint16_t*>(slot_base + (i-1) * sizeof(uint16_t));
        *dest = *src;
        // dest = 8190 + 2 = 8192 (but we'll update free_end, so this becomes valid)
        // src  = 8190 + 0 = 8190 ✓
    }
    
    // STEP 3: Write new slot using frozen base
    uint16_t* new_slot = reinterpret_cast<uint16_t*>(slot_base + index * sizeof(uint16_t));
    *new_slot = record_offset;
    
    // STEP 4: Update free_end AFTER all memory operations
    header->free_end -= sizeof(uint16_t);  // Now 8188
    header->cell_count += 1;
}
```

**Why this works**: We compute all addresses relative to the **old** `free_end` (8190), perform all operations, then update `free_end` to reflect the new state. The addresses are consistent because they're all computed from the same base.

**Memory layout**:
```
Before: free_end = 8190
  [8190]: Slot[0] = 12
  
After shift (using base 8190):
  [8190]: Slot[0] = 12 (source)
  [8192]: Slot[1] = 12 (destination - but 8192 is beyond page!)
  
Wait - this still has the problem! We need to account for the fact that
slots should be at [8188] and [8190] after insertion, not [8190] and [8192].
```

**Correction for approach (b)**: We need to compute addresses for the **final** state:

```cpp
void insert_slot(Page& page, uint16_t index, uint16_t record_offset) {
    PageHeader* header = get_header(page);
    
    uint16_t current_count = header->cell_count;
    uint16_t new_free_end = header->free_end - sizeof(uint16_t);  // Future free_end
    
    // Compute addresses for FINAL state (after free_end update)
    uint8_t* final_slot_base = page.data + new_free_end;  // Base = 8188
    
    // Shift: slot[i] moves to slot[i+1] in final layout
    for(int i = current_count - 1; i >= index; --i) {
        // Source: current position of slot[i]
        uint16_t* src = reinterpret_cast<uint16_t*>(
            page.data + header->free_end + i * sizeof(uint16_t));  // 8190 + 0 = 8190
        
        // Dest: final position of slot[i+1]
        uint16_t* dest = reinterpret_cast<uint16_t*>(
            final_slot_base + (i+1) * sizeof(uint16_t));  // 8188 + 2 = 8190
        
        *dest = *src;
    }
    
    // Write new slot at final position
    uint16_t* new_slot = reinterpret_cast<uint16_t*>(
        final_slot_base + index * sizeof(uint16_t));  // 8188 + 2 = 8190
    *new_slot = record_offset;
    
    header->free_end = new_free_end;  // Update to 8188
    header->cell_count += 1;
}
```

---

## Why Mixing Approaches Causes Silent Corruption

**Dangerous mixed approach**:
```cpp
void insert_slot(Page& page, uint16_t index, uint16_t record_offset) {
    PageHeader* header = get_header(page);
    
    // Update free_end
    header->free_end -= sizeof(uint16_t);  // free_end = 8188
    
    // But then use OLD free_end in calculations
    uint8_t* old_base = page.data + (header->free_end + sizeof(uint16_t));  // 8190
    
    // Shift using old base
    for(int i = header->cell_count; i > index; --i) {
        uint16_t* dest = reinterpret_cast<uint16_t*>(old_base + i * sizeof(uint16_t));
        // dest = 8190 + 2 = 8192 (WRONG - beyond page!)
        uint16_t* src = reinterpret_cast<uint16_t*>(old_base + (i-1) * sizeof(uint16_t));
        // src = 8190 + 0 = 8190 (correct)
        *dest = *src;  // Writes to invalid memory!
    }
}
```

**What happens**:
1. `free_end` is updated to 8188
2. But slot addresses are computed using old base (8190)
3. Destination addresses point beyond page boundary (8192)
4. **Silent corruption**: Write succeeds (no bounds check), corrupts memory
5. Later reads use `free_end=8188`, compute addresses correctly, but read corrupted data

**The corruption is "silent"** because:
- No immediate crash (memory might be writable)
- `slot_ptr()` calls later use updated `free_end`, so they compute "valid" addresses
- But the data was written to wrong locations, so reads return garbage

---

## Key Takeaways

1. **Invariant**: `slot[i]` address = `free_end + i * sizeof(uint16_t)` is only valid when `free_end` reflects the current slot count.

2. **Approach (a) - Update first**: Update `free_end` before any `slot_ptr()` calls. All addresses computed consistently.

3. **Approach (b) - Freeze base**: Compute all addresses relative to a frozen base (either old or new `free_end`), but be consistent - don't mix old and new.

4. **Mixing is dangerous**: Using updated `free_end` in header but old base in calculations causes writes to wrong addresses, leading to silent corruption.

5. **Memory safety**: Always ensure computed offsets are within `[0, PAGE_SIZE-1]`. The formula `free_end + i*2` can exceed `PAGE_SIZE` if `free_end` hasn't been updated to reflect slot growth.

