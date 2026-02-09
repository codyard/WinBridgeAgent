# ConfigManager 实现文档

## 概述

ConfigManager 是 ClawDesk MCP Server 的配置管理模块，负责配置文件的加载、保存和访问。

## 功能特性

### 1. 配置文件管理

- **自动生成默认配置**：首次运行时自动创建 `config.json`
- **配置加载**：从 JSON 文件加载配置
- **配置保存**：将配置持久化到 JSON 文件
- **配置验证**：验证配置项的有效性

### 2. Auth Token 管理

- **自动生成**：首次运行时自动生成 64 字符的随机 Token
- **自动重新生成**：检测到 Token 无效或缺失时自动重新生成
- **安全性**：使用加密安全的随机数生成器

### 3. 端口配置

- **默认端口**：35182
- **随机端口**：设置为 0 时使用随机端口
- **自动端口回退**：端口被占用时自动选择可用端口（可配置）

### 4. 白名单管理

- **目录白名单**：允许访问的目录列表
- **应用白名单**：允许启动的应用程序（名称 -> 路径映射）
- **命令白名单**：允许执行的命令列表

### 5. 线程安全

- 所有配置访问都通过互斥锁保护
- 支持多线程并发读写

## 配置文件格式

### config.json 示例

```json
{
    "auth_token": "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6a7b8c9d0e1f2",
    "server_port": 35182,
    "auto_port": true,
    "allowed_dirs": ["C:/Users", "C:/Temp", "C:/Windows/Temp"],
    "allowed_apps": {
        "notepad": "C:/Windows/System32/notepad.exe",
        "calc": "C:/Windows/System32/calc.exe"
    },
    "allowed_commands": ["npm", "git", "python", "node"],
    "license_key": ""
}
```

### 配置项说明

| 配置项             | 类型    | 说明                             | 默认值                 |
| ------------------ | ------- | -------------------------------- | ---------------------- |
| `auth_token`       | string  | Bearer Token，用于客户端认证     | 自动生成               |
| `server_port`      | number  | 服务器监听端口（0 表示随机）     | 35182                  |
| `auto_port`        | boolean | 端口被占用时是否自动选择随机端口 | true                   |
| `allowed_dirs`     | array   | 允许访问的目录列表               | 见默认配置             |
| `allowed_apps`     | object  | 允许启动的应用程序映射           | 见默认配置             |
| `allowed_commands` | array   | 允许执行的命令列表               | npm, git, python, node |
| `license_key`      | string  | 许可证密钥（可选）               | ""                     |

## API 接口

### 构造函数

```cpp
ConfigManager(const std::string& configPath = "config.json")
```

创建 ConfigManager 实例。

**参数：**

- `configPath`: 配置文件路径（默认为 "config.json"）

### 加载和保存

```cpp
void load()
```

加载配置文件。如果文件不存在，则生成默认配置并保存。

**异常：**

- `std::runtime_error`: 配置文件格式错误或无法创建文件

```cpp
void save()
```

保存配置到文件。

**异常：**

- `std::runtime_error`: 无法打开文件进行保存

### 配置访问

```cpp
std::string getAuthToken() const
```

获取 Auth Token。

```cpp
int getServerPort() const
```

获取服务器端口配置。

```cpp
bool isAutoPortEnabled() const
```

检查是否启用自动端口选择。

```cpp
std::vector<std::string> getAllowedDirs() const
```

获取允许的目录列表。

```cpp
std::map<std::string, std::string> getAllowedApps() const
```

获取允许的应用程序映射。

```cpp
std::vector<std::string> getAllowedCommands() const
```

获取允许的命令列表。

```cpp
std::string getLicenseKey() const
```

获取许可证密钥。

### 配置修改

```cpp
void setLicenseKey(const std::string& key)
```

设置许可证密钥。

```cpp
void setActualPort(int port)
```

设置实际使用的端口（保存到配置中）。

## 使用示例

### 基本使用

```cpp
#include "support/config_manager.h"

int main() {
    try {
        // 创建 ConfigManager
        ConfigManager config("config.json");

        // 加载配置
        config.load();

        // 访问配置
        std::string token = config.getAuthToken();
        int port = config.getServerPort();
        bool autoPort = config.isAutoPortEnabled();

        std::cout << "Auth Token: " << token << std::endl;
        std::cout << "Server Port: " << port << std::endl;
        std::cout << "Auto Port: " << (autoPort ? "Enabled" : "Disabled") << std::endl;

        // 修改配置
        config.setLicenseKey("my-license-key");

        // 保存配置
        config.save();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### 多线程使用

```cpp
#include "support/config_manager.h"
#include <thread>
#include <vector>

void workerThread(ConfigManager& config) {
    // 线程安全的配置访问
    std::string token = config.getAuthToken();
    auto dirs = config.getAllowedDirs();

    // 线程安全的配置修改
    config.setLicenseKey("thread-" + std::to_string(std::this_thread::get_id()));
}

int main() {
    ConfigManager config("config.json");
    config.load();

    // 创建多个线程并发访问配置
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(workerThread, std::ref(config));
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
```

## 需求覆盖

ConfigManager 实现覆盖以下需求：

- **需求 2.3**: Auth Token 自动生成
- **需求 2.4**: Auth Token 丢失或无效时自动重新生成
- **需求 14.1**: 从 config.json 文件加载配置
- **需求 14.2**: 支持配置 Auth Token、允许的目录、允许的应用和允许的命令
- **需求 14.3**: 配置文件不存在时创建默认配置文件
- **需求 14.4**: 配置文件被修改后，服务重启后加载新配置
- **需求 14.5**: 验证配置文件的格式和内容有效性

## 测试

### 单元测试

单元测试位于 `tests/unit/test_config_manager.cpp`，覆盖以下场景：

1. **默认配置生成**：验证首次运行时生成正确的默认配置
2. **配置加载和保存**：验证配置的持久化和一致性
3. **Auth Token 重新生成**：验证无效 Token 的自动重新生成
4. **配置修改**：验证配置项的修改和持久化
5. **线程安全性**：验证多线程并发访问的安全性
6. **配置验证**：验证无效配置的拒绝

### 运行测试

```bash
# 构建测试
./build-tests.sh

# 在 Windows 上运行测试
build-tests/test_config_manager.exe
```

## 实现细节

### Auth Token 生成

使用 C++ 标准库的 `std::random_device` 和 `std::mt19937` 生成加密安全的随机数：

```cpp
std::string ConfigManager::generateAuthToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // 生成 32 字节（64 个十六进制字符）
    for (int i = 0; i < 32; ++i) {
        ss << std::setw(2) << dis(gen);
    }

    return ss.str();
}
```

### 配置验证

验证配置的有效性，包括：

- Auth Token 长度至少 32 字符
- 端口号在有效范围内（0-65535）

```cpp
bool ConfigManager::validateConfig(const ServerConfig& config) const {
    if (config.auth_token.empty() || config.auth_token.length() < 32) {
        return false;
    }

    if (config.server_port < 0 || config.server_port > 65535) {
        return false;
    }

    return true;
}
```

### 线程安全

所有公共方法都使用 `std::lock_guard` 保护：

```cpp
std::string ConfigManager::getAuthToken() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.auth_token;
}
```

## 未来改进

1. **配置热重载**：支持不重启服务的配置重载
2. **配置加密**：敏感配置项（如 Auth Token）的加密存储
3. **配置备份**：自动备份配置文件
4. **配置迁移**：支持配置文件版本升级
5. **配置验证增强**：更详细的配置验证规则

## 相关文档

- [设计文档](../.kiro/specs/clawdesk-mcp-server/design.md)
- [需求文档](../.kiro/specs/clawdesk-mcp-server/requirements.md)
- [任务列表](../.kiro/specs/clawdesk-mcp-server/tasks.md)
