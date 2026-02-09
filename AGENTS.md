# Repository Guidelines

## Project Structure & Module Organization
- `src/`: C++ implementation. Entry point is `src/main.cpp`, with modules under `src/mcp`, `src/policy`, `src/services`, `src/support`, `src/utils`.
- `include/`: Public headers mirroring the `src/` layout.
- `tests/`: CMake-based tests with `tests/unit` and `tests/integration`.
- `resources/`: Runtime assets and templates, including `resources/config.template.json`.
- `third_party/`: Vendored dependencies (e.g., `cpp-httplib`, `nlohmann-json`).
- `build/x64/`, `build/x86/`: Generated build outputs.

## Build, Test, and Development Commands
- `./scripts/build.sh`: Cross-compiles Windows x64/x86 using MinGW-w64 on macOS.
- Manual build (x64 example):
  ```bash
  mkdir -p build/x64
  cd build/x64
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw-x64.cmake -DCMAKE_BUILD_TYPE=Release
  make -j$(sysctl -n hw.ncpu)
  ```
- Test build (x64 example):
  ```bash
  cmake -S tests -B build-tests-x64 -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw-x64.cmake -DCMAKE_BUILD_TYPE=Release
  cmake --build build-tests-x64
  ```
  Tests produce Windows executables, so run them in a Windows VM or with Wine.

## Coding Style & Naming Conventions
- C++17 is required (see `CMakeLists.txt`).
- Indentation is 4 spaces, braces on the same line, and header guards in headers.
- File naming favors snake_case (e.g., `config_manager.cpp`).
- Types use `PascalCase`, methods use `camelCase`, constants use `UPPER_SNAKE_CASE`.
- No formatter is enforced; match existing style in touched files.

## Testing Guidelines
- Unit tests live in `tests/unit` and use `test_*.cpp` naming.
- Add new tests by defining a new executable in `tests/CMakeLists.txt` and listing required source files.
- Prefer small, focused tests for each support/service module.

## Commit & Pull Request Guidelines
- No Git history is present in this checkout; use concise, imperative commit subjects (e.g., "Add audit log rotation").
- PRs should include: a clear summary, linked issues if any, and test evidence (command + result). For tray/UI changes, include screenshots.

## Security & Configuration Tips
- Runtime settings are stored in `config.json`; do not hardcode tokens or paths.
- Update `resources/config.template.json` when adding new configuration fields.
- Keep `allowed_dirs`, `allowed_apps`, and `allowed_commands` as minimal as possible.
