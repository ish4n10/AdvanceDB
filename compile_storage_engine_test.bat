@echo off
REM Compile and run StorageEngine test (Windows)

g++ -std=c++17 -I./include ^
    tests/storage/storage_engine_test.cpp ^
    src/storage/disk_manager.cpp ^
    src/storage/page.cpp ^
    src/storage/record.cpp ^
    src/storage/table.cpp ^
    src/storage/slot_helpers.cpp ^
    src/storage/buffer_pool.cpp ^
    src/storage/btree/btree.cpp ^
    src/storage/btree/leaf.cpp ^
    src/storage/btree/internal.cpp ^
    src/storage/btree/helpers.cpp ^
    src/storage/interface/storage_engine.cpp ^
    -o test_storage_engine.exe

if %ERRORLEVEL% EQU 0 (
    echo Running test...
    test_storage_engine.exe
) else (
    echo Compilation failed!
    exit /b 1
)
