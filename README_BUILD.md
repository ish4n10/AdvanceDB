# Building and Running with CMake

## Quick Start (Easiest Way)

**Using the PowerShell script** (Windows):
```powershell
# Build only
.\build.ps1

# Build and run test
.\build.ps1 -Run

# Clean build directory
.\build.ps1 -Clean
```

## Manual Build Steps

1. **Create a build directory** (recommended):
   ```powershell
   mkdir build
   cd build
   ```

2. **Configure CMake**:
   ```powershell
   cmake ..
   ```

3. **Build the project**:
   ```powershell
   cmake --build .
   ```

4. **Run the validation test**:
   ```powershell
   .\bin\test_validation.exe
   ```

## Alternative: Single Command

You can also build and run in one go:
```bash
cmake --build . && ./bin/test_validation
```

Or use the custom target:
```bash
cmake --build . --target run_validation
```

## Adding New Tests

To add a new test, simply add it to `CMakeLists.txt`:
```cmake
add_executable(test_your_test 
    tests/your_test/test.cpp
    ${STORAGE_SOURCES}
)
```

