# 多电脑支持功能 - v0.4.2

## 概述

v0.4.2 版本增加了多电脑支持功能，允许 AI 助手通过 MCP 配置区分和管理局域网内的多台 Windows 电脑。

## 问题背景

当局域网内有多台电脑都安装了 ClawDesk MCP Server 时，AI 助手需要知道：

1. 有哪些电脑可用
2. 每台电脑的名称和 IP 地址
3. 如何选择正确的目标电脑执行操作

## 解决方案

### 方案：MCP 配置扩展属性

在 MCP 客户端配置文件中，为每台电脑创建独立的服务器配置，使用不同的名称、URL 和 Token。

**优势**：

- ✅ 无需修改服务器代码
- ✅ 配置简单直观
- ✅ AI 助手可以直接识别电脑名称
- ✅ 支持添加自定义元数据
- ✅ 灵活扩展

## 实现细节

### 1. 服务器端改进

在 `/status` API 响应中添加 `computer_name` 字段：

**代码变更** (`src/main.cpp`):

```cpp
// 获取电脑名称
char computerName[MAX_COMPUTERNAME_LENGTH + 1];
DWORD size = sizeof(computerName);
if (!GetComputerNameA(computerName, &size)) {
    strcpy(computerName, "Unknown");
}

// 添加到响应中
sprintf(response + 512,
    "{"
    "\"status\":\"running\","
    "\"version\":\"" CLAWDESK_VERSION "\","
    "\"computer_name\":\"%s\","  // 新增字段
    "\"port\":%d,"
    ...
    "}",
    computerName, port, ...);
```

**响应示例**:

```json
{
    "status": "running",
    "version": "0.4.2",
    "computer_name": "TEST-PC",
    "port": 35182,
    "listen_address": "0.0.0.0",
    "local_ip": "192.168.31.3",
    "license": "free",
    "uptime_seconds": 3600
}
```

### 2. MCP 配置模板

创建了两个配置模板：

#### 单电脑配置 (`configs/mcp-config-template.json`)

```json
{
    "mcpServers": {
        "clawdesk": {
            "url": "http://localhost:35182",
            "transport": "http",
            "headers": {
                "Authorization": "Bearer YOUR_AUTH_TOKEN_HERE"
            },
            "description": "ClawDesk MCP Server - Windows 本地能力服务"
        }
    }
}
```

#### 多电脑配置 (`configs/mcp-config-multi-computer.json`)

```json
{
    "mcpServers": {
        "clawdesk-test": {
            "url": "http://192.168.31.3:35182",
            "transport": "http",
            "headers": {
                "Authorization": "Bearer TOKEN_FOR_TEST_PC"
            },
            "description": "ClawDesk MCP Server - Test 电脑 (192.168.31.3)",
            "metadata": {
                "computer_name": "Test",
                "ip_address": "192.168.31.3",
                "location": "办公室",
                "os": "Windows 11"
            }
        },
        "clawdesk-office": {
            "url": "http://192.168.31.4:35182",
            "transport": "http",
            "headers": {
                "Authorization": "Bearer TOKEN_FOR_OFFICE_PC"
            },
            "description": "ClawDesk MCP Server - Office 电脑 (192.168.31.4)",
            "metadata": {
                "computer_name": "Office",
                "ip_address": "192.168.31.4",
                "location": "办公室",
                "os": "Windows 10"
            }
        },
        "clawdesk-home": {
            "url": "http://192.168.31.5:35182",
            "transport": "http",
            "headers": {
                "Authorization": "Bearer TOKEN_FOR_HOME_PC"
            },
            "description": "ClawDesk MCP Server - Home 电脑 (192.168.31.5)",
            "metadata": {
                "computer_name": "Home",
                "ip_address": "192.168.31.5",
                "location": "家里",
                "os": "Windows 11"
            }
        }
    }
}
```

### 3. 配置字段说明

#### 必需字段

| 字段                  | 说明                                | 示例                        |
| --------------------- | ----------------------------------- | --------------------------- |
| 服务器名称            | MCP 服务器的唯一标识符              | `clawdesk-test`             |
| url                   | 电脑的 IP 地址和端口                | `http://192.168.31.3:35182` |
| headers.Authorization | Bearer Token（从 config.json 获取） | `Bearer a1b2c3d4...`        |

#### 可选字段

| 字段                   | 说明           | 示例                       |
| ---------------------- | -------------- | -------------------------- |
| description            | 电脑的描述信息 | `Test 电脑 (192.168.31.3)` |
| metadata.computer_name | 电脑名称       | `Test`                     |
| metadata.ip_address    | IP 地址        | `192.168.31.3`             |
| metadata.location      | 物理位置       | `办公室`                   |
| metadata.os            | 操作系统       | `Windows 11`               |

## 使用流程

### 1. 在每台电脑上部署 ClawDesk MCP Server

```bash
# 使用构建脚本部署到多台电脑
./scripts/build.sh
```

构建脚本支持配置多个部署目标：

```bash
DEPLOY_TARGETS=(
    "Test|/Volumes/Test|192.168.31.3|35182"
    "Office|/Volumes/Office|192.168.31.4|35182"
    "Home|/Volumes/Home|192.168.31.5|35182"
)
```

### 2. 获取每台电脑的 Token

在每台电脑上：

```bash
# 查看 config.json
cat config.json | grep auth_token
```

或者通过 Settings 窗口查看和复制 Token。

### 3. 配置 MCP 客户端

编辑 MCP 客户端配置文件（如 Claude Desktop）：

- macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
- Windows: `%APPDATA%\Claude\claude_desktop_config.json`

添加多电脑配置。

### 4. 重启 MCP 客户端

重启 Claude Desktop 或其他 MCP 客户端。

### 5. 使用 AI 助手

```
User: 在 Test 电脑上截图

AI: 好的，我来在 Test 电脑上截图...
[调用 clawdesk-test 的 take_screenshot 工具]
已完成，截图保存在 Test 电脑的 screenshots 目录。
```

## AI 助手使用示例

### 示例 1: 指定电脑执行操作

```
User: 在 Office 电脑上列出 C:\Users 目录

AI: [调用 clawdesk-office 的 list_directory 工具]
Office 电脑的 C:\Users 目录包含：
- Administrator
- Public
- User1
```

### 示例 2: 查询所有电脑的状态

```
User: 查询所有电脑的状态

AI:
Test 电脑 (192.168.31.3):
- 版本: 0.4.2
- 运行时间: 2小时
- 状态: 正常

Office 电脑 (192.168.31.4):
- 版本: 0.4.2
- 运行时间: 5小时
- 状态: 正常

Home 电脑 (192.168.31.5):
- 版本: 0.4.2
- 运行时间: 1小时
- 状态: 正常
```

### 示例 3: 在多台电脑上执行相同操作

```
User: 在所有电脑上截图

AI: 好的，我来在所有电脑上截图...

Test 电脑: ✓ 截图完成
Office 电脑: ✓ 截图完成
Home 电脑: ✓ 截图完成

所有截图已保存。
```

### 示例 4: 根据位置选择电脑

```
User: 在办公室的电脑上打开记事本

AI: 根据配置，办公室有两台电脑：Test 和 Office。
我将在 Test 电脑上打开记事本...
[调用 clawdesk-test 的 execute_command 工具]
已在 Test 电脑上打开记事本。
```

## 网络拓扑

```
局域网 (192.168.31.0/24)
│
├─ Test 电脑 (192.168.31.3:35182)
│  └─ ClawDesk MCP Server v0.4.2
│     └─ Token: a1b2c3d4e5f6...
│
├─ Office 电脑 (192.168.31.4:35182)
│  └─ ClawDesk MCP Server v0.4.2
│     └─ Token: x9y8z7w6v5u4...
│
├─ Home 电脑 (192.168.31.5:35182)
│  └─ ClawDesk MCP Server v0.4.2
│     └─ Token: m5n4o3p2q1r0...
│
└─ AI 助手 (macOS/Windows)
   └─ Claude Desktop / MCP 客户端
      └─ 配置了 3 个 MCP 服务器
         ├─ clawdesk-test
         ├─ clawdesk-office
         └─ clawdesk-home
```

## 文档更新

### 新增文档

1. **`docs/MULTI_COMPUTER_SETUP.md`** - 多电脑配置详细指南
    - 配置方案说明
    - 使用流程
    - AI 助手使用示例
    - 故障排除
    - 安全建议

2. **`configs/mcp-config-multi-computer.json`** - 多电脑配置模板
    - 包含 3 台电脑的示例配置
    - 详细的字段说明
    - metadata 扩展示例

### 更新文档

1. **`README.md`** - 添加多电脑部署章节
2. **`CHANGELOG.md`** - 记录多电脑支持功能
3. **`src/main.cpp`** - 添加 computer_name 字段

## 技术实现

### 获取电脑名称

使用 Windows API `GetComputerNameA()` 获取电脑名称：

```cpp
char computerName[MAX_COMPUTERNAME_LENGTH + 1];
DWORD size = sizeof(computerName);
if (!GetComputerNameA(computerName, &size)) {
    strcpy(computerName, "Unknown");
}
```

### 响应格式

在 `/status` API 中添加 `computer_name` 字段：

```json
{
  "status": "running",
  "version": "0.4.2",
  "computer_name": "TEST-PC",
  ...
}
```

## 优势

1. **简单直观**: 通过 MCP 配置即可区分多台电脑
2. **灵活扩展**: 可以添加任意自定义元数据
3. **无需修改**: 不需要修改服务器端代码
4. **AI 友好**: AI 助手可以直接识别电脑名称
5. **安全隔离**: 每台电脑使用独立的 Token

## 限制

1. **手动配置**: 需要手动配置每台电脑的 Token
2. **静态配置**: 添加或删除电脑需要重启 MCP 客户端
3. **局域网限制**: 仅支持局域网内的电脑（可通过 VPN 扩展）

## 未来改进

- [ ] 支持动态发现局域网内的 ClawDesk 服务器
- [ ] 支持服务器端配置电脑别名
- [ ] 支持电脑分组管理
- [ ] 支持批量操作 API
- [ ] 支持远程电脑（VPN/公网）

## 版本信息

- **版本**: v0.4.2
- **发布日期**: 2026-02-05
- **构建时间**: 11:03
- **新增功能**: 多电脑支持、computer_name 字段

## 相关文档

- [多电脑配置指南](docs/MULTI_COMPUTER_SETUP.md)
- [MCP 协议文档](docs/MCP.md)
- [API 文档](docs/API.md)
- [README](README.md)
- [更新日志](CHANGELOG.md)

## 测试建议

### 1. 测试 computer_name 字段

```bash
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.31.3:35182/status | jq .computer_name
```

应该返回电脑名称，如 `"TEST-PC"`。

### 2. 测试多电脑配置

配置 3 台电脑后，使用 AI 助手测试：

```
User: 列出所有可用的电脑

AI: 当前可用的电脑有：
1. Test 电脑 (192.168.31.3)
2. Office 电脑 (192.168.31.4)
3. Home 电脑 (192.168.31.5)
```

### 3. 测试指定电脑操作

```
User: 在 Test 电脑上截图

AI: [应该调用 clawdesk-test 的截图工具]
```

## 总结

v0.4.2 版本通过 MCP 配置扩展属性实现了多电脑支持，允许 AI 助手轻松管理和操作局域网内的多台 Windows 电脑。配置简单、使用方便、扩展灵活。
