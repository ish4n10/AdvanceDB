@echo off
REM Build script for AdvanceDB using g++
REM Builds: advance_db.exe, advance_db_client.exe, advance_db_server.exe

echo ========================================
echo Building AdvanceDB with g++
echo ========================================
echo.

REM Create bin directory if it doesn't exist
if not exist bin mkdir bin

REM Common compiler flags
set CXXFLAGS=-std=c++17 -I./include -I./include/storage_new -I./include/parser/statements
set LDFLAGS=-lws2_32

REM Storage_new sources
set STORAGE_NEW_SOURCES=src/storage_new/catalog_manager.cpp src/storage_new/schema_serializer.cpp src/storage_new/storage_manager.cpp src/storage_new/storage.cpp src/storage_new/db_manager.cpp src/storage_new/transaction_manager.cpp

REM Network sources
REM winsock_utils.cpp contains shared init_winsock() and cleanup_winsock() functions
set NETWORK_SOURCES=src/network/winsock_utils.cpp src/network/tcp_client.cpp src/network/tcp_server.cpp src/network/select_server.cpp

REM Parser sources
REM Note: statements.cpp includes parser.cpp, so we only compile parser.cpp directly
REM when NOT including statements.cpp (which includes parser.cpp internally)
set PARSER_SOURCES=src/parser/parser.cpp

REM ========================================
REM Build 1: advance_db.exe (standalone)
REM ========================================
REM Note: orchestrator.cpp includes statements.cpp which includes parser.cpp,
REM so we don't compile parser.cpp separately
echo [1/3] Building advance_db.exe...
g++ %CXXFLAGS% ^
    src/main.cpp ^
    %STORAGE_NEW_SOURCES% ^
    src/analyser/analyser.cpp ^
    src/orchestrator/orchestrator.cpp ^
    %LDFLAGS% ^
    -o bin/advance_db.exe

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build advance_db.exe
    exit /b 1
)
echo [OK] advance_db.exe built successfully
echo.

REM ========================================
REM Build 2: advance_db_client.exe
REM ========================================
echo [2/3] Building advance_db_client.exe...
g++ %CXXFLAGS% ^
    src/client/main.cpp ^
    %NETWORK_SOURCES% ^
    %LDFLAGS% ^
    -o bin/advance_db_client.exe

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build advance_db_client.exe
    exit /b 1
)
echo [OK] advance_db_client.exe built successfully
echo.

REM ========================================
REM Build 3: advance_db_server.exe
REM ========================================
REM Note: orchestrator.cpp includes statements.cpp which includes parser.cpp,
REM so we don't compile parser.cpp separately
echo [3/3] Building advance_db_server.exe...
g++ %CXXFLAGS% ^
    src/server/main.cpp ^
    %STORAGE_NEW_SOURCES% ^
    %NETWORK_SOURCES% ^
    src/analyser/analyser.cpp ^
    src/orchestrator/orchestrator.cpp ^
    %LDFLAGS% ^
    -o bin/advance_db_server.exe

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build advance_db_server.exe
    exit /b 1
)
echo [OK] advance_db_server.exe built successfully
echo.

REM ========================================
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executables are in: bin\
echo   - bin\advance_db.exe (standalone)
echo   - bin\advance_db_client.exe (client)
echo   - bin\advance_db_server.exe (server)
echo.
echo To run:
echo   Standalone: bin\advance_db.exe
echo   Server:     bin\advance_db_server.exe 5432
echo   Client:     bin\advance_db_client.exe
echo ========================================
