# Debugging in VS Code

## Quick Start

1. **Set breakpoints** - Click in the gutter (left of line numbers) in `tests/page/validation.cpp` or any source file

2. **Start debugging** - Press `F5` or click "Run and Debug" in the sidebar, then select "Debug Validation Test"

3. **Debug controls**:
   - `F5` - Continue
   - `F10` - Step Over
   - `F11` - Step Into
   - `Shift+F11` - Step Out
   - `Ctrl+Shift+F5` - Restart
   - `Shift+F5` - Stop

## Debug Configurations

### "Debug Validation Test"
- Full build: Configures CMake (if needed) then builds
- Best for: First-time debugging or after CMake changes

### "Debug Validation Test (Current File)" 
- Fast build: Just builds, skips CMake configure
- Best for: Quick debugging iterations

## Tips

- **Breakpoints**: Click in the left gutter to set breakpoints
- **Watch Variables**: Add variables to the Watch panel to monitor values
- **Call Stack**: See the function call hierarchy
- **Debug Console**: Execute expressions and print values

## Troubleshooting

If debugging doesn't work:
1. Make sure CMake is configured: `cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..`
2. Build the project: `cmake --build . --config Debug`
3. Check that `build/bin/test_validation.exe` exists

