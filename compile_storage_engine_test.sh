#!/bin/bash
# Compile and run StorageEngine test

g++ -std=c++17 -I./include \
    tests/storage/storage_engine_test.cpp \
    src/storage/disk_manager.cpp \
    src/storage/page.cpp \
    src/storage/record.cpp \
    src/storage/table.cpp \
    src/storage/slot_helpers.cpp \
    src/storage/buffer_pool.cpp \
    src/storage/btree/btree.cpp \
    src/storage/btree/leaf.cpp \
    src/storage/btree/internal.cpp \
    src/storage/btree/helpers.cpp \
    src/storage/interface/storage_engine.cpp \
    -o test_storage_engine.exe

# Run the test
./test_storage_engine.exe
