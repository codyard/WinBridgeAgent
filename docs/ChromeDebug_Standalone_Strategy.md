# Chrome Debug MCP - Standalone Deployment Strategy

## Overview

This document outlines the strategy for extracting the Chrome Debug functionality from ClawDesk MCP server into a standalone MCP server in the future.

## Current Architecture (Integrated)

```
ClawDesk MCP Server
├── Chrome Debug Service (integrated)
├── File Operations Service
├── Process Management Service
├── Screenshot Service
├── Power Management Service
└── Shared Infrastructure
    ├── ConfigManager
    ├── AuditLogger
    ├── PolicyGuard
    └── ToolRegistry
```

**Benefits**:

- Single executable deployment
- Unified configuration and security
- Shared audit logging
- Faster initial development

## Future Architecture (Standalone)

```
Chrome Debug MCP Server (standalone)
├── Chrome Debug Service (extracted)
├── Lightweight Infrastructure
│   ├── Simple Config Loader
│   ├── Simple Logger
│   └── Simple Security Controller
└── MCP Protocol Handler
```

**Benefits**:

- Lighter weight (smaller executable)
- Independent versioning
- Specialized deployment
- Easier distribution
- Can run alongside ClawDesk or independently

## Modular Components (Already Independent)

These components are designed to be extracted with minimal changes:

### 1. WebSocketClient

**Location**: `src/support/websocket_client.cpp`, `include/support/websocket_client.h`

**Dependencies**: None (pure Winsock2 implementation)

**Extraction**: Copy as-is to standalone project

### 2. CDPConnection

**Location**: `src/support/cdp_connection.cpp`, `include/support/cdp_connection.h`

**Dependencies**: WebSocketClient, nlohmann/json

**Extraction**: Copy as-is to standalone project

### 3. ChromeDebugService

**Location**: `src/services/chrome_debug_service.cpp`, `include/services/chrome_debug_service.h`

**Dependencies**: CDPConnection, Config interface, Logger interface, Security interface

**Extraction**: Copy with interface adapters (see below)

## Dependency Abstraction Interfaces

To support future extraction, we use dependency injection with abstract interfaces:

### Configuration Interface

**Current (ClawDesk)**:

```cpp
class ChromeDebugService {
    ConfigManager* configManager;  // ClawDesk's ConfigManager
};
```

**Future (Standalone)**:

```cpp
// Create simple interface
class IChromeDebugConfig {
    virtual int getDebugPort() = 0;
    virtual std::vector<std::string> getAllowedDomains() = 0;
    virtual int getCommandTimeout() = 0;
    // ... other config methods
};

// Implement lightweight adapter
class SimpleChromeDebugConfig : public IChromeDebugConfig {
    // Load from simple JSON file
};

class ChromeDebugService {
    IChromeDebugConfig* config;  // Interface, not concrete class
};
```

### Logging Interface

**Current (ClawDesk)**:

```cpp
class ChromeDebugService {
    clawdesk::AuditLogger* auditLogger;  // ClawDesk's AuditLogger
};
```

**Future (Standalone)**:

```cpp
// Create simple interface
class IChromeDebugLogger {
    virtual void logOperation(const std::string& operation, const std::string& details) = 0;
    virtual void logError(const std::string& error) = 0;
};

// Implement lightweight adapter
class SimpleChromeDebugLogger : public IChromeDebugLogger {
    // Write to simple log file or stdout
};

class ChromeDebugService {
    IChromeDebugLogger* logger;  // Interface, not concrete class
};
```

### Security Interface

**Current (ClawDesk)**:

```cpp
class ChromeDebugService {
    PolicyGuard* policyGuard;  // ClawDesk's PolicyGuard
};
```

**Future (Standalone)**:

```cpp
// Create simple interface
class IChromeDebugSecurity {
    virtual bool isUrlAllowed(const std::string& url) = 0;
    virtual bool isScriptAllowed(const std::string& script) = 0;
    virtual bool checkRateLimit(const std::string& operation) = 0;
};

// Implement lightweight adapter
class SimpleChromeDebugSecurity : public IChromeDebugSecurity {
    // Simple domain whitelist and rate limiting
};

class ChromeDebugService {
    IChromeDebugSecurity* security;  // Interface, not concrete class
};
```

## Extraction Steps (Future)

When ready to create standalone Chrome Debug MCP server:

### Step 1: Create New Project Structure

```
chrome-debug-mcp/
├── src/
│   ├── main.cpp                    # New standalone main
│   ├── mcp_handler.cpp             # MCP protocol handler
│   ├── simple_config.cpp           # Lightweight config
│   ├── simple_logger.cpp           # Lightweight logger
│   ├── simple_security.cpp         # Lightweight security
│   ├── chrome_debug_service.cpp    # Copied from ClawDesk
│   ├── cdp_connection.cpp          # Copied from ClawDesk
│   └── websocket_client.cpp        # Copied from ClawDesk
├── include/
│   ├── interfaces/
│   │   ├── i_chrome_debug_config.h
│   │   ├── i_chrome_debug_logger.h
│   │   └── i_chrome_debug_security.h
│   ├── chrome_debug_service.h      # Copied from ClawDesk
│   ├── cdp_connection.h            # Copied from ClawDesk
│   └── websocket_client.h          # Copied from ClawDesk
├── config.json                     # Simple config file
└── CMakeLists.txt                  # Standalone build
```

### Step 2: Implement Lightweight Adapters

Create simple implementations of the abstraction interfaces:

**simple_config.cpp**: Load from JSON file
**simple_logger.cpp**: Write to file or stdout
**simple_security.cpp**: Basic domain whitelist and rate limiting

### Step 3: Create Standalone Main

```cpp
// main.cpp for standalone Chrome Debug MCP server
int main() {
    // Initialize lightweight components
    SimpleChromeDebugConfig config("config.json");
    SimpleChromeDebugLogger logger("chrome_debug.log");
    SimpleChromeDebugSecurity security(&config);

    // Initialize Chrome Debug Service
    ChromeDebugService chromeDebug(&config, &logger, &security);

    // Start MCP protocol handler
    MCPHandler mcpHandler(&chromeDebug);
    mcpHandler.start();

    return 0;
}
```

### Step 4: Update ChromeDebugService

Minimal changes needed:

```cpp
// Change from concrete classes to interfaces
class ChromeDebugService {
public:
    ChromeDebugService(
        IChromeDebugConfig* config,      // Interface instead of ConfigManager
        IChromeDebugLogger* logger,      // Interface instead of AuditLogger
        IChromeDebugSecurity* security   // Interface instead of PolicyGuard
    );

    // Rest of the implementation stays the same
};
```

### Step 5: Build and Package

```bash
# Build standalone executable
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make

# Result: chrome-debug-mcp.exe (much smaller than ClawDesk)
```

## Configuration Comparison

### ClawDesk (Integrated)

```json
{
  "chrome_debug": {
    "enabled": true,
    "debug_port": 9222,
    "allowed_domains": ["*.example.com"],
    "timeouts": { "connection": 5000, "command": 10000 }
  },
  "file_operations": { ... },
  "process_management": { ... }
}
```

### Standalone

```json
{
    "debug_port": 9222,
    "debug_host": "localhost",
    "allowed_domains": ["*.example.com", "*.test.com"],
    "blocked_script_patterns": ["eval\\(", "Function\\("],
    "timeouts": {
        "connection": 5000,
        "command": 10000,
        "script_execution": 30000,
        "page_load": 60000
    },
    "rate_limits": {
        "commands_per_minute": 100,
        "scripts_per_minute": 20
    },
    "logging": {
        "level": "info",
        "file": "chrome_debug.log"
    }
}
```

## Deployment Scenarios

### Scenario 1: Integrated (Current)

**Use Case**: Users want full ClawDesk functionality including Chrome automation

**Deployment**: Single ClawDesk executable with Chrome Debug enabled

**Configuration**: chrome_debug section in ClawDesk config.json

### Scenario 2: Standalone (Future)

**Use Case**: Users only need browser automation, want lightweight deployment

**Deployment**: Separate chrome-debug-mcp.exe

**Configuration**: Standalone config.json

### Scenario 3: Both (Future)

**Use Case**: Users want both, running on different ports

**Deployment**:

- ClawDesk on port 3000 (Chrome Debug disabled)
- chrome-debug-mcp on port 3001

**Configuration**: Separate config files

## Migration Path

For users who want to migrate from integrated to standalone:

1. **Export Configuration**: Extract chrome_debug section from ClawDesk config
2. **Install Standalone**: Download chrome-debug-mcp.exe
3. **Configure**: Create standalone config.json
4. **Update MCP Client**: Point to new port (if different)
5. **Disable in ClawDesk**: Set chrome_debug.enabled = false in ClawDesk config

## Development Timeline

**Phase 1 (Current)**: Integrated implementation

- Develop Chrome Debug functionality in ClawDesk
- Use dependency injection pattern
- Keep components modular
- **Timeline**: 2-3 months

**Phase 2 (Future)**: Standalone extraction

- Create standalone project structure
- Implement lightweight adapters
- Test standalone deployment
- Package and distribute
- **Timeline**: 2-4 weeks (when needed)

## Conclusion

The current integrated approach provides:

- Faster development (reuse existing infrastructure)
- Unified security and logging
- Single deployment for users

The modular design ensures:

- Future extraction is straightforward
- Minimal refactoring required
- Both deployment options can coexist
- Users can choose based on their needs

This strategy gives us the best of both worlds: rapid development now, flexibility later.
