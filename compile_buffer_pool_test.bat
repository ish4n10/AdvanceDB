@echo off
REM Compile and run BufferPool test (Windows)

g++ -std=c++17 -I./include ^
    tests/storage/buffer_pool_test.cpp ^
    src/storage/disk_manager.cpp ^
    src/storage/page.cpp ^
    src/storage/record.cpp ^
    src/storage/table.cpp ^
    src/storage/slot_helpers.cpp ^
    src/storage/buffer_pool.cpp ^
    -o test_buffer_pool.exe

if %ERRORLEVEL% EQU 0 (
    echo Running test...
    test_buffer_pool.exe
) else (
    echo Compilation failed!
    exit /b 1
)
