# WinBridgeAgent

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows)](https://github.com/codyard/WinBridgeAgent)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![MCP Protocol](https://img.shields.io/badge/MCP-Model%20Context%20Protocol-green)](https://modelcontextprotocol.io)
[![GitHub Release](https://img.shields.io/github/v/release/codyard/WinBridgeAgent?include_prereleases&sort=semver)](https://github.com/codyard/WinBridgeAgent/releases)
[![GitHub Stars](https://img.shields.io/github/stars/codyard/WinBridgeAgent?style=social)](https://github.com/codyard/WinBridgeAgent)

[中文文档](README_zh.md)

WinBridgeAgent is an open-source Windows local capability service that implements the Model Context Protocol (MCP) standard, providing traceable Windows system operations for AI assistants such as Claude Desktop, OpenAI, etc.

## Features

### Core

- **MCP Protocol**: Full Model Context Protocol implementation
- **HTTP API**: HTTP interface for remote control
- **Real-time Monitoring**: Dashboard window with live request/response display
- **Audit Logging**: Complete operation logging
- **System Tray**: Convenient graphical management interface
- **Flexible Configuration**: Switch between 0.0.0.0 and 127.0.0.1 listen address
- **Auto Update**: Automatic GitHub release checking with one-click download

### File & System Operations

- **File Reading**: Disk enumeration, directory listing, file reading (line ranges), content search
- **File Operations**: Delete, copy, move, create directories
- **Clipboard**: Read/write clipboard content via HTTP API (text, images, files)
- **Screenshots**:
  - Full screen (HTTP API, PNG/JPEG)
  - Window capture (MCP tool, by window title)
  - Region capture (MCP tool, by coordinates)

### Process & Window Management

- **Window Management**: List all open windows with details
- **Process Queries**: List all running processes with details
- **Process Management**: Terminate processes, adjust priority (with protected process blocklist)
- **Command Execution**: Execute system commands and capture output

### Power Management

- **Shutdown**: Delayed, forced, with custom message
- **Restart**: Delayed, forced
- **Hibernate & Sleep**: System hibernate and sleep
- **Cancel Shutdown**: Cancel scheduled shutdown/restart

### Auto Update

- **Version Check**: Automatic GitHub release checking
- **Update Notification**: Friendly notification dialog
- **One-click Download**: Open GitHub Release page
- **Multi-channel**: Stable and Beta channels
- **Multi-language**: Simplified Chinese, English

## System Requirements

- Windows 10/11 (x64, x86, ARM64)
- No additional runtime dependencies

## Quick Start

1. Download the latest release from [GitHub Releases](https://github.com/codyard/WinBridgeAgent/releases)
2. Run `WinBridgeAgent-x64.exe` (64-bit) or `WinBridgeAgent-x86.exe` (32-bit)
3. The program appears in the system tray
4. Access the API at `http://localhost:35182`

## HTTP API

The server listens on the configured port (default 35182).

> **Open-source edition**: No Bearer Token authentication required. All API endpoints are directly accessible.

### Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | API endpoint list |
| GET | `/health` | Health check |
| GET | `/status` | Server status and info |
| GET | `/disks` | List all disk drives |
| GET | `/list?path=<path>` | List directory contents |
| GET | `/read?path=<path>` | Read file content |
| GET | `/search?path=<path>&query=<q>` | Search file content |
| GET | `/clipboard` | Read clipboard |
| PUT | `/clipboard` | Write clipboard |
| GET | `/screenshot` | Take screenshot |
| GET | `/windows` | List all windows |
| GET | `/processes` | List all processes |
| POST | `/execute` | Execute command |
| GET | `/exit` | Shutdown server |
| POST | `/mcp/initialize` | MCP initialize |
| POST | `/mcp/tools/list` | MCP list tools |
| POST | `/mcp/tools/call` | MCP call tool |

### Examples

```bash
# Server status
curl http://localhost:35182/status

# List disks
curl http://localhost:35182/disks

# List directory
curl "http://localhost:35182/list?path=C:\\"

# Read file
curl "http://localhost:35182/read?path=C:\\test.txt"

# Search file content
curl "http://localhost:35182/search?path=C:\\test.txt&query=keyword"

# Read clipboard
curl http://localhost:35182/clipboard

# Write clipboard
curl -X PUT -H "Content-Type: application/json" \
  -d '{"content":"Hello World"}' http://localhost:35182/clipboard

# Take screenshot
curl http://localhost:35182/screenshot

# List windows
curl http://localhost:35182/windows

# List processes
curl http://localhost:35182/processes

# Execute command
curl -X POST -H "Content-Type: application/json" \
  -d '{"command":"echo Hello"}' http://localhost:35182/execute

# MCP: Initialize
curl -X POST -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}' \
  http://localhost:35182/mcp/initialize

# MCP: List tools
curl -X POST -H "Content-Type: application/json" \
  -d '{}' http://localhost:35182/mcp/tools/list

# MCP: Call tool
curl -X POST -H "Content-Type: application/json" \
  -d '{"name":"list_windows","arguments":{}}' \
  http://localhost:35182/mcp/tools/call
```

## Multi-Computer Setup

If you have multiple computers with WinBridgeAgent installed on your LAN, configure your MCP client to manage them:

```json
{
    "mcpServers": {
        "winbridge-test": {
            "url": "http://192.168.31.3:35182",
            "description": "Test PC"
        },
        "winbridge-office": {
            "url": "http://192.168.31.4:35182",
            "description": "Office PC"
        }
    }
}
```

See [Multi-Computer Setup Guide](docs/MULTI_COMPUTER_SETUP.md) for details.

## Configuration

A `config.json` file is auto-generated on first run:

```json
{
    "server_port": 35182,
    "auto_port": true,
    "listen_address": "0.0.0.0",
    "language": "en",
    "auto_startup": false,
    "daemon_enabled": true
}
```

| Key | Description | Default |
|-----|-------------|---------|
| `server_port` | Server listen port | `35182` |
| `auto_port` | Auto-select port if occupied | `true` |
| `listen_address` | `0.0.0.0` (LAN) or `127.0.0.1` (local only) | `0.0.0.0` |
| `language` | UI language (`en` / `zh-CN`) | `en` |
| `auto_startup` | Start with Windows | `false` |
| `daemon_enabled` | Enable daemon watchdog | `true` |

## Building from Source

### Prerequisites

- macOS (Apple Silicon or Intel)
- CMake 3.20+
- MinGW-w64 cross-compilation toolchain

### Install Dependencies

```bash
brew install mingw-w64 cmake
```

### Build

```bash
# Build all architectures
./scripts/build.sh

# Or build a single architecture manually
mkdir -p build/x64
cd build/x64
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x64.cmake \
            -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### Build Outputs

- `build/x64/WinBridgeAgent.exe` — Windows x64
- `build/x86/WinBridgeAgent.exe` — Windows x86
- `build/arm64/WinBridgeAgent.exe` — Windows ARM64

## Project Structure

```text
.
├── CMakeLists.txt
├── src/            # C++ source code
├── include/        # Public headers
├── resources/      # Runtime assets and templates
├── scripts/        # Build, test, release scripts
├── docs/           # Documentation
├── tests/          # Test code
├── third_party/    # Vendored dependencies
└── build/          # Build outputs
```

## Security Notes

- **Listen Address**: Setting `0.0.0.0` allows LAN access; use `127.0.0.1` for local-only
- **Firewall**: Windows Firewall may block network access; add an exception if needed
- **Protected Processes**: System-critical processes cannot be terminated (system, csrss.exe, winlogon.exe, etc.)
- **Protected Directories**: System directories cannot be deleted (C:\Windows, C:\Program Files, etc.)
- **Audit Log**: All operations are logged

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).

Copyright © 2026 WinBridgeAgent Contributors
