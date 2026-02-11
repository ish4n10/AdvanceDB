# Storage Engine with Relational Layer

## Overview

The storage engine provides a relational database interface on top of a B+tree key-value storage layer. It supports tables with schemas, typed columns, primary keys, and row-level operations (insert, scan, schema queries).

## Architecture

```
┌─────────────────────────────────────────┐
│     Relational API (StorageEngine)      │
│  - create_table(schema)                 │
│  - insert(table, row)                   │
│  - scan(table) → rows                   │
│  - has_table, get_schema                │
└─────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
┌───────▼────────┐    ┌─────────▼──────────┐
│    Catalog     │    │    RowCodec       │
│  (schemas)     │    │  (encode/decode)  │
└────────────────┘    └───────────────────┘
        │                       │
        └───────────┬───────────┘
                    │
        ┌───────────▼───────────┐
        │   Key-Value API       │
        │  - insert_record      │
        │  - scan_table         │
        │  - get_record         │
        └───────────────────────┘
                    │
        ┌───────────▼───────────┐
        │   B+tree Storage     │
        │  - One B+tree/table  │
        │  - Key = Primary Key │
        │  - Value = Full Row  │
        └───────────────────────┘
```

## Components

### 1. Catalog (`include/storage/relational/catalog.hpp`)

In-memory registry of table schemas.

**Schema Definition:**
```cpp
namespace Relational {
    enum class ColumnType {
        INT, FLOAT, DOUBLE, STRING, BOOLEAN, DATETIME
    };
    
    struct ColumnDef {
        std::string name;
        ColumnType type;
    };
    
    struct TableSchema {
        int pk_index;  // Which column is the primary key
        std::vector<ColumnDef> columns;
    };
}
```

**API:**
- `register_table(name, schema)` - Register a table schema
- `get_schema(name)` - Get schema for a table
- `has_table(name)` - Check if table exists
- `drop_table(name)` - Remove table from catalog

### 2. RowCodec (`include/storage/relational/row_codec.hpp`)

Encodes/decodes typed tuples to/from byte arrays.

**Types:**
```cpp
namespace Relational {
    using Value = std::variant<int, float, double, std::string, bool>;
    using Tuple = std::vector<Value>;  // One row = vector of values
}
```

**Encoding:**
- **Key**: Primary key column only (for B+tree key)
- **Value**: All columns (for B+tree value)
- Uses type tags (1 byte per column) + data

**API:**
- `encode_key(tuple)` - Encode PK column → bytes
- `encode_value(tuple)` - Encode all columns → bytes
- `decode(data)` - Decode bytes → tuple

### 3. StorageEngine (`include/storage/interface/storage_engine.hpp`)

Main relational API that combines catalog, codec, and key-value storage.

**Relational API:**
```cpp
class StorageEngine {
    // Table management
    bool create_table(const std::string& name, const Relational::TableSchema& schema);
    bool drop_table(const std::string& name);
    bool has_table(const std::string& name) const;
    const Relational::TableSchema* get_schema(const std::string& name) const;
    
    // Row operations
    bool insert(const std::string& table_name, const Relational::Tuple& row);
    std::vector<Relational::Tuple> scan(const std::string& table_name);
    
    // Key-value API (lower level)
    bool insert_record(TableHandle* handle, const std::vector<uint8_t>& key, 
                       const std::vector<uint8_t>& value);
    void scan_table(TableHandle* handle, ScanCallback callback, void* ctx);
    // ... more KV operations
};
```

## How It Works

### Table Creation

1. `create_table(name, schema)`:
   - Registers schema in `Catalog`
   - Creates B+tree file: `data/<name>.db`
   - Initializes root page (empty B+tree)

### Row Insertion

1. `insert(table_name, row)`:
   - Gets schema from `Catalog`
   - Uses `RowCodec` to encode:
     - **Key** = primary key column only
     - **Value** = all columns
   - Calls `insert_record(table_handle, key_bytes, value_bytes)`
   - B+tree inserts: `key → value` entry

### Row Scanning

1. `scan(table_name)`:
   - Gets schema from `Catalog`
   - Opens table B+tree
   - Calls `scan_table()` with callback
   - Callback decodes each value → tuple
   - Returns vector of tuples

### Storage Layout

**On Disk:**
- One file per table: `data/<table_name>.db`
- File contains B+tree pages (slotted-page format)
- Root page ID stored in page 0 (meta page)

**In Memory:**
- `Catalog` holds schemas (in-memory only)
- `StorageEngine` caches open `TableHandle`s
- Buffer pool manages page cache

## Usage Example

```cpp
#include "storage/interface/storage_engine.hpp"
#include "storage/relational/catalog.hpp"

StorageEngine engine;

// Define schema
Relational::TableSchema schema;
schema.pk_index = 0;  // First column is PK
schema.columns = {
    {"id", Relational::ColumnType::INT},
    {"name", Relational::ColumnType::STRING},
    {"age", Relational::ColumnType::INT}
};

// Create table
engine.create_table("users", schema);

// Insert rows
Relational::Tuple row1 = {1, std::string("Alice"), 25};
Relational::Tuple row2 = {2, std::string("Bob"), 30};
engine.insert("users", row1);
engine.insert("users", row2);

// Scan all rows
auto rows = engine.scan("users");
for (const auto& row : rows) {
    int id = std::get<int>(row[0]);
    std::string name = std::get<std::string>(row[1]);
    int age = std::get<int>(row[2]);
    // Process row...
}

// Query schema
const Relational::TableSchema* s = engine.get_schema("users");
// s->columns[0].name == "id", s->pk_index == 0

// Drop table
engine.drop_table("users");
```

## File Structure

```
include/storage/
├── interface/
│   └── storage_engine.hpp      # Main relational API
├── relational/
│   ├── catalog.hpp              # Schema registry
│   └── row_codec.hpp           # Tuple encoding/decoding
├── btree.hpp                    # B+tree operations
├── page.hpp                     # Page layout
├── record.hpp                   # Record operations
└── table_handle.hpp             # Table handle

src/storage/
├── interface/
│   └── storage_engine.cpp       # StorageEngine implementation
├── relational/
│   ├── catalog.cpp               # Catalog implementation
│   └── row_codec.cpp            # Encoding/decoding logic
├── btree/                        # B+tree implementation
├── page.cpp                      # Page operations
└── table.cpp                     # Table file management
```

## Key Design Decisions

1. **One B+tree per table**: Each table is stored as a B+tree keyed by primary key
2. **Schema-driven encoding**: `RowCodec` uses schema to know column types
3. **In-memory catalog**: Schemas stored in memory (not persisted yet)
4. **Type-safe tuples**: Uses `std::variant` for typed values
5. **Separation of concerns**: Relational layer is thin wrapper over key-value API

## Current Limitations

- No secondary indexes (only primary key)
- Catalog not persisted (schemas lost on restart)
- No UPDATE/DELETE in relational API (only INSERT/SCAN)
- No transactions or concurrency control
- No query optimizer (always full table scan)

## Running Tests

### Build Tests

From the project root:

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

Or use the build script:
```bash
./build.ps1  # PowerShell
# or
./build.ps1  # Bash (if PowerShell available)
```

### Available Tests

**Relational Engine Test** (tests relational API):
```bash
# From build directory
cmake --build . --target run_relational_engine_test

# Or run directly
./bin/test_relational_engine.exe
```

**Storage Engine Test** (tests key-value API):
```bash
cmake --build . --target run_storage_engine_test
# Or: ./bin/test_storage_engine.exe
```

**B+tree Test** (tests B+tree operations):
```bash
cmake --build . --target run_btree_test
# Or: ./bin/test_btree.exe
```

**Page Tests**:
```bash
cmake --build . --target run_validation
cmake --build . --target run_page_allocation
```

### Test Files

- `tests/storage/relational_engine_test.cpp` - Relational API (create_table, insert, scan)
- `tests/storage/storage_engine_test.cpp` - Key-value API (insert_record, get_record, etc.)
- `tests/storage/btree_test/btree_test.cpp` - B+tree operations (insert, search, delete, splits)
- `tests/page/page_insert.cpp` - Page-level record insertion
- `tests/page/page_allocation.cpp` - Page allocation and management

### Running from Project Root

Tests expect to run from the project root (for `data/` directory):

```bash
# PowerShell
cd g:\advancedb\AdvanceDB
.\build\bin\test_relational_engine.exe

# Bash
cd /g/advancedb/AdvanceDB
./build/bin/test_relational_engine.exe
```

**Note**: On Windows with MinGW, ensure MinGW's `bin` directory is on PATH, or run via CMake targets which handle DLL loading automatically.

## Future Extensions

- Secondary indexes (separate B+trees per index)
- Persist catalog to disk
- Add UPDATE/DELETE operations
- Query planner/optimizer
- Transaction support
