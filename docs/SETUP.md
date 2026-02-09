# ClawDesk MCP Server - Setup Documentation

## Project Initialization Complete ✓

This document summarizes the project initialization and infrastructure setup.

### Completed Tasks

#### 1. Project Structure

- ✓ Created CMake project structure
- ✓ Created directory hierarchy:
    - `include/` - Header files (mcp, policy, services, support, utils)
    - `src/` - Source files (mcp, policy, services, support, utils)
    - `tests/` - Unit and integration tests
    - `resources/` - Configuration templates and icons
    - `third_party/` - External dependencies

#### 2. Third-Party Libraries

- ✓ Integrated cpp-httplib (header-only HTTP server)
- ✓ Integrated nlohmann/json (modern C++ JSON library)
- ✓ Integrated stb_image_write (PNG image writing)

#### 3. Cross-Compilation Toolchain

- ✓ Installed MinGW-w64 via Homebrew
- ✓ Created toolchain files:
    - `toolchain-mingw-x64.cmake` - Windows x64 target
    - `toolchain-mingw-x86.cmake` - Windows x86 target
    - `toolchain-mingw-arm64.cmake` - Windows ARM64 target (compiler not available)

#### 4. Build Configuration

- ✓ Created `CMakeLists.txt` with:
    - C++17 standard
    - Static linking of MinGW runtime (-static-libgcc, -static-libstdc++, -static)
    - Windows API libraries (ws2_32, gdi32, user32, shell32, advapi32)
    - Release optimization (-O3, -s)
    - No console window (WIN32_EXECUTABLE)

#### 5. Build Script

- ✓ Created `../scripts/build.sh` for automated building
- ✓ Supports x64 and x86 architectures
- ✓ Gracefully skips ARM64 if compiler unavailable
- ✓ Parallel compilation using all CPU cores

#### 6. Verification

- ✓ Created Hello World test program
- ✓ Successfully compiled x64 version (972KB)
- ✓ Successfully compiled x86 version (941KB)
- ✓ Verified PE executable format
- ✓ Verified static linking (only Windows system DLLs)

#### 7. Project Files

- ✓ Created `.gitignore`
- ✓ Created `README.md`
- ✓ Created `LICENSE`
- ✓ Created `resources/config.template.json`

### Build Results

```
x64: build/x64/ClawDeskMCP.exe (972KB)
x86: build/x86/ClawDeskMCP.exe (941KB)
```

Both executables are:

- PE format (Windows executables)
- Statically linked (no external DLL dependencies except Windows system DLLs)
- Optimized for release (-O3)
- Stripped of debug symbols (-s)

### Dependencies

The executables only depend on Windows system DLLs:

- KERNEL32.dll
- Universal CRT (api-ms-win-crt-\*.dll)

These are present on all Windows 10/11 systems.

### Next Steps

The project infrastructure is ready for implementation:

1. Implement ConfigManager module
2. Implement AuditLogger module
3. Implement LicenseManager module
4. Implement PolicyGuard module
5. Implement capability services (Screenshot, File, Clipboard, etc.)
6. Implement MCP protocol layer
7. Implement system tray manager

### Development Workflow

```bash
# Clean build all architectures
../scripts/build.sh

# Or build specific architecture
mkdir -p build/x64
cd build/x64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw-x64.cmake \
         -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Testing

Since we're cross-compiling from macOS to Windows:

- Cannot run executables directly on macOS
- Need Windows VM or physical machine for testing
- Consider setting up Wine for basic testing (optional)

### Notes

- ARM64 compiler not available in current MinGW-w64 installation
- Can be added later if needed
- x64 and x86 cover the vast majority of Windows systems
