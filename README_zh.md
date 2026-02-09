# WinBridgeAgent

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows)](https://github.com/codyard/WinBridgeAgent)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![MCP Protocol](https://img.shields.io/badge/MCP-Model%20Context%20Protocol-green)](https://modelcontextprotocol.io)
[![GitHub Release](https://img.shields.io/github/v/release/codyard/WinBridgeAgent?include_prereleases&sort=semver)](https://github.com/codyard/WinBridgeAgent/releases)
[![GitHub Stars](https://img.shields.io/github/stars/codyard/WinBridgeAgent?style=social)](https://github.com/codyard/WinBridgeAgent)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-io.github.codyard%2Fwinbridgeagent-orange)](https://registry.modelcontextprotocol.io/v0.1/servers?search=io.github.codyard/winbridgeagent)

[English](README.md)

WinBridgeAgent 是一个开源的 Windows 本地能力服务，遵循 Model Context Protocol (MCP) 标准协议，为 AI 助理（如 Claude Desktop、OpenAI 等）提供可追溯的 Windows 系统操作能力。

## 架构

```
+------------------------------------------------------+
|                Local Area Network (LAN)              |
|                                                      |
|   +--------------------+                             |
|   |   MCP Client       |                             |
|   | (OpenClaw / Agent) |                             |
|   |  - LLM reasoning   |                             |
|   |  - Tool planning   |                             |
|   +---------+----------+                             |
|             | MCP Protocol (HTTP/SSE)                |
|             |                                        |
|   +---------v------------------------------------+   |
|   |           Windows PC                          |   |
|   |                                              |   |
|   |   +--------------------------------------+   |   |
|   |   |  WinBridgeAgent                      |   |   |
|   |   |  (Local Tray MCP Server)             |   |   |
|   |   |                                      |   |   |
|   |   |  - MCP Server (localhost / LAN)      |   |   |
|   |   |  - Policy & Permission Guard         |   |   |
|   |   |  - Tool Execution Layer              |   |   |
|   |   |  - Audit & Logging                   |   |   |
|   |   +-----------+--------------------------+   |   |
|   |               |                              |   |
|   |      +--------v--------+                     |   |
|   |      | Windows System  |                     |   |
|   |      | APIs / Resources|                     |   |
|   |      | (FS / Screen /  |                     |   |
|   |      |  Clipboard /    |                     |   |
|   |      |  Process etc.)  |                     |   |
|   |      +-----------------+                     |   |
|   |                                              |   |
|   +----------------------------------------------+   |
|                                                      |
+------------------------------------------------------+

Optional External Dependencies:
- GitHub Releases (Auto Update)
- Optional Remote Services (APIs, Cloud tools)
```

## 功能特性

### 核心功能

- **MCP 协议支持**: 实现 Model Context Protocol 标准协议
- **HTTP API**: 提供 HTTP 接口用于远程控制
- **实时监控**: Dashboard 窗口实时显示请求和处理过程
- **审计追溯**: 完整的操作日志记录
- **系统托盘**: 便捷的图形化管理界面
- **灵活配置**: 支持 0.0.0.0 或 127.0.0.1 监听地址切换
- **自动更新**: 自动检查 GitHub 新版本，一键下载更新

### 文件和系统操作

- **文件读取**: 磁盘枚举、目录列表、文件读取（支持行范围）、内容搜索
- **文件操作** (v0.3.0): 文件删除、复制、移动、目录创建
- **剪贴板操作**: 通过 HTTP API 读写剪贴板内容（支持文本、图片、文件）
- **截图功能**:
    - 全屏截图（HTTP API，支持 PNG/JPEG 格式）
    - 指定窗口截图（MCP 工具，通过窗口标题匹配）
    - 指定区域截图（MCP 工具，通过坐标和尺寸）

### 进程和窗口管理

- **窗口管理**: 获取所有打开窗口的列表和详细信息
- **进程查询**: 获取所有运行进程的列表和详细信息
- **进程管理** (v0.3.0): 终止进程、调整进程优先级（支持受保护进程黑名单）
- **命令执行**: 执行系统命令并捕获输出

### 电源管理 (v0.3.0)

- **系统关机**: 支持延迟关机、强制关机、自定义消息
- **系统重启**: 支持延迟重启、强制重启
- **休眠和睡眠**: 系统休眠和睡眠功能
- **取消关机**: 取消计划的关机或重启操作

### 自动更新 (v0.5.0)

- **版本检查**: 自动从 GitHub 检查最新版本
- **更新通知**: 发现新版本时显示友好的通知对话框
- **一键下载**: 自动打开 GitHub Release 页面下载
- **多通道支持**: 支持 Stable（稳定版）和 Beta（测试版）通道
- **配置灵活**: 可配置检查间隔、GitHub 仓库等
- **多语言**: 支持简体中文、英语等多种语言

详细说明请参考 [自动更新文档](docs/AutoUpdate.md)

## 系统要求

- Windows 10/11 (x64, x86, ARM64)
- 无需额外运行时依赖

## 快速开始

### 从 MCP Registry 安装

WinBridgeAgent 已发布到 [MCP Registry](https://registry.modelcontextprotocol.io)，名称为 `io.github.codyard/winbridgeagent`。支持 Registry 的 MCP 客户端可直接安装。

### 手动安装

1. 从 [GitHub Releases](https://github.com/codyard/WinBridgeAgent/releases) 下载最新版本
2. 运行 `WinBridgeAgent-x64.exe`（64位系统）或 `WinBridgeAgent-x86.exe`（32位系统）
3. 程序会在系统托盘显示图标
4. 配置你的 MCP 客户端（见下方）

## MCP 客户端配置

WinBridgeAgent 可与任何兼容 MCP 的客户端配合使用。以下是常用客户端的配置方法。

> **注意**: 请将 `<windows-ip>` 替换为运行 WinBridgeAgent 的 Windows 电脑的 IP 地址（如 `192.168.1.100`）。仅当 MCP 客户端运行在同一台 Windows 机器上时才使用 `localhost`。

### Claude Desktop

编辑 `claude_desktop_config.json`：

- **Windows**: `%APPDATA%\Claude\claude_desktop_config.json`
- **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
    "mcpServers": {
        "winbridgeagent": {
            "url": "http://<windows-ip>:35182",
            "transport": "http"
        }
    }
}
```

### Cursor

1. 打开 **Settings** → **MCP**
2. 点击 **Add new MCP server**
3. 填写：
   - **Name**: `winbridgeagent`
   - **Type**: `http`
   - **URL**: `http://<windows-ip>:35182`

或编辑项目根目录下的 `.cursor/mcp.json`：

```json
{
    "mcpServers": {
        "winbridgeagent": {
            "url": "http://<windows-ip>:35182",
            "transport": "http"
        }
    }
}
```

### Windsurf

编辑 `~/.codeium/windsurf/mcp_config.json`：

```json
{
    "mcpServers": {
        "winbridgeagent": {
            "serverUrl": "http://<windows-ip>:35182"
        }
    }
}
```

### OpenClaw

1. 打开 **设置** → **MCP 服务器**
2. 点击 **添加**
3. 设置：
   - **名称**: `winbridgeagent`
   - **传输方式**: `HTTP`
   - **URL**: `http://<windows-ip>:35182`

### Cherry Studio

1. 打开 **设置** → **MCP 服务器**
2. 点击 **添加服务器**
3. 选择 **Streamable HTTP** 类型
4. 设置 URL 为 `http://<windows-ip>:35182`

### Cline (VS Code)

编辑 VS Code 设置中的 `cline_mcp_settings.json`：

```json
{
    "mcpServers": {
        "winbridgeagent": {
            "url": "http://<windows-ip>:35182",
            "transportType": "http"
        }
    }
}
```

### 通用 MCP 客户端

任何支持 HTTP 传输的 MCP 客户端均可使用以下信息连接：

- **服务器 URL**: `http://<windows-ip>:35182`
- **传输方式**: HTTP (Streamable HTTP)
- **协议版本**: `2024-11-05`

## 多电脑部署

如果你在局域网内有多台电脑都安装了 WinBridgeAgent，可以通过 MCP 配置文件来管理和区分不同的电脑。

**快速配置**:

```json
{
    "mcpServers": {
        "winbridge-test": {
            "url": "http://192.168.31.3:35182",
            "description": "Test 电脑"
        },
        "winbridge-office": {
            "url": "http://192.168.31.4:35182",
            "description": "Office 电脑"
        }
    }
}
```

AI 助手可以根据电脑名称选择目标：

```
User: 在 Test 电脑上截图
AI: [调用 clawdesk-test 的截图工具]
```

**详细配置指南**: 参见 [多电脑配置指南](docs/MULTI_COMPUTER_SETUP.md)

## 项目结构

```text
.
├── AGENTS.md
├── CLAUDE.md
├── CMakeLists.txt
├── configs/
├── docs/
├── resources/
├── scripts/
├── src/
├── include/
├── tests/
├── third_party/
└── build/
```

说明：

- `configs/`: 配置模板与示例
- `docs/`: 项目文档与指南
- `resources/`: 运行时资源与模板（包含 `resources/config.template.json`）
- `scripts/`: 构建、测试、发布脚本（见 `scripts/README.md`）
- `src/`: C++ 实现源码
- `include/`: 公共头文件
- `tests/`: 测试代码
- `third_party/`: 第三方依赖
- `build/`: 生成产物（如 `build/x64`, `build/x86`）

## HTTP API

服务器启动后会在配置的端口（默认 35182）上监听 HTTP 请求。

> **开源版说明**: 开源版无需 Bearer Token 认证，所有 API 端点均可直接访问。

### 可用端点

#### 1. API 列表

```bash
GET http://<windows-ip>:35182/
```

返回所有可用的 API 端点列表。

#### 2. 健康检查

```bash
GET http://<windows-ip>:35182/health
```

响应：

```json
{ "status": "ok" }
```

#### 3. 获取状态

```bash
GET http://<windows-ip>:35182/status
```

响应：

```json
{
    "status": "running",
    "version": "0.3.0",
    "port": 35182,
    "listen_address": "0.0.0.0",
    "local_ip": "192.168.31.3",
    "license": "opensource",
    "uptime_seconds": 1234
}
```

#### 4. 获取磁盘列表

```bash
GET http://<windows-ip>:35182/disks
```

响应：

```json
[
    {
        "drive": "C:",
        "type": "fixed",
        "label": "Windows",
        "filesystem": "NTFS",
        "total_bytes": 500000000000,
        "free_bytes": 100000000000,
        "used_bytes": 400000000000
    }
]
```

#### 5. 列出目录内容

```bash
GET http://<windows-ip>:35182/list?path=C:\Users
```

响应：

```json
[
    {
        "name": "Documents",
        "type": "directory",
        "size": 0,
        "modified": "2026-02-03 10:30:00"
    },
    {
        "name": "file.txt",
        "type": "file",
        "size": 1024,
        "modified": "2026-02-03 12:00:00"
    }
]
```

#### 6. 读取文件内容

```bash
# 读取整个文件
GET http://<windows-ip>:35182/read?path=C:\test.txt

# 从第 10 行开始读取 20 行
GET http://<windows-ip>:35182/read?path=C:\test.txt&start=10&lines=20

# 读取最后 50 行
GET http://<windows-ip>:35182/read?path=C:\test.txt&tail=50

# 只获取行数
GET http://<windows-ip>:35182/read?path=C:\test.txt&count=true
```

响应：

```json
{
    "path": "C:\\test.txt",
    "total_lines": 100,
    "start_line": 0,
    "returned_lines": 100,
    "file_size": 5120,
    "content": "文件内容..."
}
```

#### 7. 搜索文件内容

```bash
# 区分大小写搜索
GET http://<windows-ip>:35182/search?path=C:\test.txt&query=keyword

# 不区分大小写搜索
GET http://<windows-ip>:35182/search?path=C:\test.txt&query=keyword&case=i

# 限制结果数量
GET http://<windows-ip>:35182/search?path=C:\test.txt&query=keyword&max=50
```

响应：

```json
{
    "path": "C:\\test.txt",
    "query": "keyword",
    "total_lines": 100,
    "match_count": 5,
    "case_sensitive": true,
    "matches": [
        {
            "line_number": 10,
            "content": "This line contains keyword"
        }
    ]
}
```

#### 8. 读取剪贴板

**增强功能**：支持文本、图片和文件三种类型！

```bash
GET http://<windows-ip>:35182/clipboard
```

响应类型 1 - 文本：

```json
{
    "type": "text",
    "content": "剪贴板文本内容",
    "length": 15,
    "empty": false
}
```

响应类型 2 - 图片（截图或复制的图片）：

```json
{
    "type": "image",
    "format": "png",
    "url": "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png",
    "path": "/clipboard/image/clipboard_images/clipboard_20260203_223015.png"
}
```

响应类型 3 - 文件（从资源管理器复制的文件）：

```json
{
    "type": "files",
    "files": [
        {
            "name": "20260203_223015_0_document.pdf",
            "url": "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf",
            "path": "/clipboard/file/20260203_223015_0_document.pdf"
        }
    ]
}
```

**下载剪贴板图片或文件**：

```bash
# 下载图片
curl "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png" \
  -o clipboard_image.png

# 下载文件
curl "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf" \
  -o document.pdf
```

#### 9. 写入剪贴板

```bash
PUT http://<windows-ip>:35182/clipboard
Content-Type: application/json

{
  "content": "要写入的内容"
}
```

响应：

```json
{
    "success": true,
    "length": 18
}
```

#### 10. 截图

##### 10.1 全屏截图（HTTP API）

```bash
# PNG 格式（默认）
GET http://<windows-ip>:35182/screenshot

# JPEG 格式
GET http://<windows-ip>:35182/screenshot?format=jpg
```

响应：

```json
{
    "success": true,
    "format": "png",
    "width": 1920,
    "height": 1080,
    "url": "http://192.168.31.3:35182/screenshot/file/screenshot_20260205_102530.png",
    "path": "/screenshot/file/screenshot_20260205_102530.png"
}
```

下载截图文件：

```bash
curl "http://192.168.31.3:35182/screenshot/file/screenshot_20260205_102530.png" \
  -o screenshot.png
```

##### 10.2 指定窗口截图（MCP 工具）

通过 MCP 协议调用 `take_screenshot_window` 工具：

```json
{
    "name": "take_screenshot_window",
    "arguments": {
        "title": "Chrome"
    }
}
```

响应：

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 1024,
    "height": 768,
    "created_at": "2026-02-05T02:25:30Z"
}
```

特性：

- 支持窗口标题模糊匹配（不区分大小写）
- 自动恢复最小化窗口
- 自动将窗口置于前台

##### 10.3 指定区域截图（MCP 工具）

通过 MCP 协议调用 `take_screenshot_region` 工具：

```json
{
    "name": "take_screenshot_region",
    "arguments": {
        "x": 100,
        "y": 100,
        "width": 800,
        "height": 600
    }
}
```

响应：

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 800,
    "height": 600,
    "created_at": "2026-02-05T02:25:30Z"
}
```

坐标系统：

- 原点 (0, 0) 在屏幕左上角
- X 轴向右增加，Y 轴向下增加

#### 11. 获取窗口列表

```bash
GET http://<windows-ip>:35182/windows
```

响应：

```json
[
    {
        "hwnd": 123456,
        "title": "Google Chrome",
        "class": "Chrome_WidgetWin_1",
        "visible": true,
        "minimized": false,
        "maximized": false,
        "process_id": 1234,
        "x": 100,
        "y": 100,
        "width": 1280,
        "height": 720
    }
]
```

#### 12. 获取进程列表

```bash
GET http://<windows-ip>:35182/processes
```

响应：

```json
[
    {
        "pid": 1234,
        "name": "chrome.exe",
        "path": "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "threads": 42,
        "parent_pid": 5678,
        "memory_kb": 102400
    }
]
```

#### 13. 执行命令

```bash
POST http://<windows-ip>:35182/execute
Content-Type: application/json

{
  "command": "dir C:\\"
}
```

响应：

```json
{
    "success": true,
    "exit_code": 0,
    "output": "Volume in drive C is Windows\\n..."
}
```

#### 14. MCP 协议

##### 初始化连接

```bash
POST http://<windows-ip>:35182/mcp/initialize
Content-Type: application/json

{
  "protocolVersion": "2024-11-05",
  "capabilities": {},
  "clientInfo": {
    "name": "test-client",
    "version": "1.0.0"
  }
}
```

响应：

```json
{
    "protocolVersion": "2024-11-05",
    "capabilities": {
        "tools": {}
    },
    "serverInfo": {
        "name": "WinBridge Agent",
        "version": "0.3.0"
    }
}
```

##### 列出可用工具

```bash
POST http://<windows-ip>:35182/mcp/tools/list
Content-Type: application/json

{}
```

响应：

```json
{
    "tools": [
        {
            "name": "read_file",
            "description": "Read file content",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string"
                    }
                },
                "required": ["path"]
            }
        }
    ]
}
```

##### 调用工具

```bash
POST http://<windows-ip>:35182/mcp/tools/call
Content-Type: application/json

{
  "name": "list_windows",
  "arguments": {}
}
```

响应：

```json
{
    "content": [
        {
            "type": "text",
            "text": "[...]"
        }
    ],
    "isError": false
}
```

**新增常用工具示例**：

```json
{
    "name": "focus_window",
    "arguments": {
        "title": "Notepad"
    }
}
```

```json
{
    "name": "set_window_state",
    "arguments": {
        "title": "Notepad",
        "action": "minimize"
    }
}
```

```json
{
    "name": "send_hotkey",
    "arguments": {
        "hotkey": "ctrl+shift+esc"
    }
}
```

```json
{
    "name": "search_files",
    "arguments": {
        "path": "C:\\Users",
        "name_query": "report",
        "content_query": "TODO",
        "exts": [".md", ".txt"],
        "days": 30,
        "max": 50
    }
}
```

#### 15. 退出服务器

```bash
GET http://<windows-ip>:35182/exit
```

响应：

```json
{ "status": "shutting down" }
```

### 使用示例

> 请将 `<windows-ip>` 替换为 Windows 电脑的实际 IP 地址（或在同一台机器上测试时使用 `localhost`）。

使用 curl 测试：

```bash
# 检查服务器状态
curl http://<windows-ip>:35182/status

# 健康检查
curl http://<windows-ip>:35182/health

# 获取磁盘列表
curl http://<windows-ip>:35182/disks

# 列出目录
curl "http://<windows-ip>:35182/list?path=C:\\"

# 读取文件
curl "http://<windows-ip>:35182/read?path=C:\\test.txt"

# 搜索文件内容
curl "http://<windows-ip>:35182/search?path=C:\\test.txt&query=keyword"

# 读取剪贴板
curl http://<windows-ip>:35182/clipboard

# 写入剪贴板
curl -X PUT -H "Content-Type: application/json" \
  -d '{"content":"Hello World"}' http://<windows-ip>:35182/clipboard

# 截图
curl http://<windows-ip>:35182/screenshot | jq -r '.url'

# 获取窗口列表
curl http://<windows-ip>:35182/windows | jq '.[0:5]'

# 获取进程列表
curl http://<windows-ip>:35182/processes | jq '.[0:5]'

# 执行命令
curl -X POST -H "Content-Type: application/json" \
  -d '{"command":"echo Hello"}' http://<windows-ip>:35182/execute | jq .

# MCP 协议：初始化
curl -X POST -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}' \
  http://<windows-ip>:35182/mcp/initialize | jq .

# MCP 协议：列出工具
curl -X POST -H "Content-Type: application/json" \
  -d '{}' http://<windows-ip>:35182/mcp/tools/list | jq '.tools | length'

# MCP 协议：调用工具
curl -X POST -H "Content-Type: application/json" \
  -d '{"name":"list_windows","arguments":{}}' \
  http://<windows-ip>:35182/mcp/tools/call | jq .

# 退出服务器
curl http://<windows-ip>:35182/exit
```

使用 PowerShell：

```powershell
# 检查状态
Invoke-WebRequest -Uri "http://<windows-ip>:35182/status"

# 读取剪贴板
Invoke-WebRequest -Uri "http://<windows-ip>:35182/clipboard"

# 写入剪贴板
$body = @{ content = "Hello World" } | ConvertTo-Json
Invoke-WebRequest -Uri "http://<windows-ip>:35182/clipboard" `
  -Method PUT -ContentType "application/json" -Body $body

# 截图
$response = Invoke-RestMethod -Uri "http://<windows-ip>:35182/screenshot"
$response.url

# 获取窗口列表
Invoke-RestMethod -Uri "http://<windows-ip>:35182/windows"

# 获取进程列表
Invoke-RestMethod -Uri "http://<windows-ip>:35182/processes"

# 执行命令
$body = @{ command = "dir C:\" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://<windows-ip>:35182/execute" `
  -Method POST -ContentType "application/json" -Body $body

# 退出服务器
Invoke-WebRequest -Uri "http://<windows-ip>:35182/exit"
```

## 托盘菜单功能

右键点击系统托盘图标可以访问以下功能：

- **状态信息**: 显示服务器运行状态、端口、监听地址
- **View Logs**: 打开日志目录
- **Dashboard**: 打开实时监控窗口（显示请求和处理过程）
- **Toggle Listen Address**: 在 0.0.0.0 和 127.0.0.1 之间切换监听地址
- **Open Config**: 用记事本打开配置文件
- **About**: 显示关于信息
- **Exit**: 退出服务器

## Dashboard 实时监控

Dashboard 是一个置顶的浮动窗口，用于实时观察服务器的运行状态。

### 功能

- **实时日志**: 显示接收的请求、处理过程和执行结果
- **日志类型**:
    - `[REQ]` - 接收到的请求
    - `[PRO]` - 正在处理
    - `[OK ]` - 成功完成
    - `[ERR]` - 发生错误
- **操作按钮**:
    - **Clear Logs**: 清空所有日志
    - **Copy All**: 复制所有日志到剪贴板
- **自动滚动**: 新日志自动滚动到底部
- **日志限制**: 最多保留 1000 条日志

### 使用方法

1. 右键点击托盘图标
2. 选择 "Dashboard"
3. Dashboard 窗口会弹出并置顶显示
4. 再次点击 "Dashboard" 可以隐藏窗口

### 日志示例

```
[18:30:45.123] [REQ] HTTP - GET /status HTTP/1.1
[18:30:45.125] [PRO] status - Retrieving server status...
[18:30:45.128] [OK ] status - Status returned: port=35182, license=free

[18:31:02.456] [REQ] HTTP - POST /exit HTTP/1.1
[18:31:02.458] [PRO] exit - Shutting down server...
[18:31:02.460] [OK ] exit - Shutdown command sent
```

详细文档请参考 [Dashboard.md](docs/Dashboard.md)。

## 配置说明

首次运行时，程序会自动生成 `config.json` 配置文件：

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

### 配置项说明

- **server_port**: 服务器监听端口（默认 35182）
- **auto_port**: 端口被占用时是否自动选择随机端口
- **listen_address**: 监听地址
  - `"0.0.0.0"`: 允许网络访问（局域网内其他设备可访问）
  - `"127.0.0.1"`: 仅本地访问
- **language**: 界面语言（en / zh-CN）
- **auto_startup**: 是否开机自启动
- **daemon_enabled**: 是否启用守护进程

## 构建说明

### 开发环境

- macOS (Apple Silicon 或 Intel)
- CMake 3.20+
- MinGW-w64 交叉编译工具链

### 安装依赖

```bash
# 安装 MinGW-w64
brew install mingw-w64

# 安装 CMake
brew install cmake
```

### 编译

```bash
# 编译所有架构（x64, x86）
./scripts/build.sh

# 或手动编译单个架构
mkdir -p build/x64
cd build/x64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw-x64.cmake \
         -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### 构建产物

- `build/x64/WinBridgeAgent.exe` - Windows x64 版本
- `build/x86/WinBridgeAgent.exe` - Windows x86 版本
- `build/arm64/WinBridgeAgent.exe` - Windows ARM64 版本

## 使用说明

1. **运行程序**：
    - 双击 `WinBridgeAgent-x64.exe`（64位系统）或 `WinBridgeAgent-x86.exe`（32位系统）
    - 程序会在系统托盘显示图标

2. **查看托盘图标**：
    - 图标会出现在任务栏右下角（系统托盘区）
    - 右键点击图标可以看到完整菜单

3. **自动生成的文件**：
    - `config.json` - 配置文件（首次运行自动生成）
    - `logs/audit-YYYY-MM-DD.log` - 审计日志

4. **网络访问**：
    - 本地访问: `http://localhost:35182`
    - 局域网访问: `http://<windows-ip>:35182`（需要 listen_address 设置为 "0.0.0.0"）

## 安全注意事项

### 网络安全

- **监听地址**: 如果设置为 `0.0.0.0`，局域网内的其他设备可以访问服务器
- **防火墙**: Windows 防火墙可能会阻止网络访问，需要添加例外

### 操作安全

- **受保护进程**: 系统关键进程无法被终止（system, csrss.exe, winlogon.exe 等）
- **系统目录**: 系统目录受保护，无法删除（C:\Windows, C:\Program Files 等）
- **审计日志**: 所有操作都会被记录

### 权限要求

- **管理员权限**: 某些操作需要管理员权限
  - Realtime 进程优先级
  - 电源管理操作（关机、重启、休眠、睡眠）
  - 某些进程操作（取决于目标进程）

## 许可证

本项目基于 [GNU General Public License v3.0](LICENSE) 开源。

Copyright © 2026 WinBridgeAgent Contributors

