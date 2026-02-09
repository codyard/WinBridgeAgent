# ClawDesk MCP Server - MCP 协议文档

## 概述

ClawDesk MCP Server 实现了 Model Context Protocol (MCP) 标准协议，使 AI 助手能够通过标准化的方式调用 Windows 系统功能。

**⚠️ 重要：从 v0.2.0 开始，所有 MCP 请求都需要 Bearer Token 认证。**

## 认证

### 获取 Token

1. 启动 ClawDesk MCP Server
2. 打开 `config.json` 文件
3. 复制 `auth_token` 字段的值

示例：

```json
{
  "auth_token": "a1b2c3d4e5f6789012345678901234567890abcdefabcdefabcdefabcdefabcd",
  ...
}
```

### 使用 Token

所有 MCP 请求都必须在 HTTP 头中包含 Authorization：

```
Authorization: Bearer YOUR_AUTH_TOKEN
```

示例：

```bash
curl -X POST \
  -H "Authorization: Bearer a1b2c3d4..." \
  -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05",...}' \
  http://localhost:35182/mcp/initialize
```

详细说明请参考 [Authentication Guide](Authentication.md)。

## MCP 协议版本

- **协议版本**: 2024-11-05
- **实现状态**: 基础实现
- **支持的功能**: 工具调用
- **工具总数**: 24 个（v0.2.0: 9 个 + v0.3.0: 8 个 + v0.4.0: 7 个）

### 工具分类

| 分类                  | 工具数量 | 工具列表                                                         |
| --------------------- | -------- | ---------------------------------------------------------------- |
| 文件读取              | 3        | read_file, search_file, list_directory                           |
| 剪贴板                | 2        | get_clipboard, set_clipboard                                     |
| 屏幕和窗口            | 3        | take_screenshot, take_screenshot_window, take_screenshot_region  |
| 窗口管理              | 4        | list_windows, focus_window, set_window_topmost, set_window_state |
| 进程查询              | 1        | list_processes                                                   |
| 命令执行              | 2        | execute_command, send_hotkey                                     |
| 文件搜索              | 1        | search_files                                                     |
| **进程管理** (v0.3.0) | 2        | kill_process, set_process_priority                               |
| **文件操作** (v0.3.0) | 4        | delete_file, copy_file, move_file, create_directory              |
| **电源管理** (v0.3.0) | 2        | shutdown_system, abort_shutdown                                  |
| **总计**              | **24**   |                                                                  |

### 风险等级分布

| 风险等级             | 工具数量 | 说明                     |
| -------------------- | -------- | ------------------------ |
| Low（低风险）        | 8        | 只读操作，无系统影响     |
| Medium（中等风险）   | 5        | 文件操作、进程优先级调整 |
| High（高风险）       | 3        | 进程终止、文件删除       |
| Critical（关键风险） | 1        | 系统关机/重启            |

## 端点

### 1. 初始化连接

建立 MCP 连接并交换能力信息。

**端点**: `POST /mcp/initialize`

**请求**:

```json
{
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "clientInfo": {
        "name": "client-name",
        "version": "1.0.0"
    }
}
```

**响应**:

```json
{
    "protocolVersion": "2024-11-05",
    "capabilities": {
        "tools": {}
    },
    "serverInfo": {
        "name": "ClawDesk MCP Server",
        "version": "0.2.0"
    }
}
```

### 2. 列出可用工具

获取服务器提供的所有工具列表。

**端点**: `POST /mcp/tools/list`

**请求**:

```json
{}
```

**响应**:

```json
{
    "tools": [
        {
            "name": "read_file",
            "description": "Read file content with optional line range",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "File path"
                    },
                    "start": {
                        "type": "number",
                        "description": "Start line (optional)"
                    },
                    "lines": {
                        "type": "number",
                        "description": "Number of lines (optional)"
                    },
                    "tail": {
                        "type": "number",
                        "description": "Read last n lines (optional)"
                    }
                },
                "required": ["path"]
            }
        }
    ]
}
```

### 3. 调用工具

执行指定的工具。

**端点**: `POST /mcp/tools/call`

**请求**:

```json
{
    "name": "list_windows",
    "arguments": {}
}
```

**响应**:

```json
{
    "content": [
        {
            "type": "text",
            "text": "[{\"hwnd\":123456,\"title\":\"Chrome\",...}]"
        }
    ],
    "isError": false
}
```

## 可用工具

ClawDesk MCP Server 提供 **35 个工具**，涵盖文件操作、系统管理、进程控制、电源管理，以及浏览器自动化（Chrome CDP）等功能。

### 基础工具 (v0.2.0) - 9 个工具

#### 1. read_file

读取文件内容。

**参数**:

- `path` (string, 必需): 文件路径
- `start` (number, 可选): 起始行号
- `lines` (number, 可选): 读取行数
- `tail` (number, 可选): 从尾部读取 n 行

**示例**:

```json
{
    "name": "read_file",
    "arguments": {
        "path": "C:\\test.txt",
        "tail": 10
    }
}
```

### 2. search_file

在文件中搜索文本。

**参数**:

- `path` (string, 必需): 文件路径
- `query` (string, 必需): 搜索关键词
- `case_sensitive` (boolean, 可选): 是否区分大小写

**示例**:

```json
{
    "name": "search_file",
    "arguments": {
        "path": "C:\\log.txt",
        "query": "error",
        "case_sensitive": false
    }
}
```

### 3. list_directory

列出目录内容。

**参数**:

- `path` (string, 必需): 目录路径

**示例**:

```json
{
    "name": "list_directory",
    "arguments": {
        "path": "C:\\Users"
    }
}
```

### 4. get_clipboard

获取剪贴板内容。

**增强功能**：支持文本、图片和文件三种类型！

**参数**: 无

**返回类型**:

- `type: "text"` - 文本内容
- `type: "image"` - 图片（返回 URL）
- `type: "files"` - 文件列表（返回 URL 数组）

**示例**:

```json
{
    "name": "get_clipboard",
    "arguments": {}
}
```

**响应示例 1 - 文本**:

```json
{
    "type": "text",
    "content": "Hello World",
    "length": 11,
    "empty": false
}
```

**响应示例 2 - 图片**:

```json
{
    "type": "image",
    "format": "png",
    "url": "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png",
    "path": "/clipboard/image/clipboard_images/clipboard_20260203_223015.png"
}
```

**响应示例 3 - 文件**:

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

**使用场景**:

- 获取用户截图（Win+Shift+S）
- 获取用户复制的文件
- 获取用户复制的文本

### 5. set_clipboard

设置剪贴板内容（仅支持文本）。

**参数**:

- `content` (string, 必需): 要设置的文本内容

**示例**:

```json
{
    "name": "set_clipboard",
    "arguments": {
        "content": "Hello World"
    }
}
```

### 6. take_screenshot

捕获全屏截图。

**风险等级**: High（高风险）

**参数**:

- `format` (string, 可选): 图像格式 (png 或 jpg)，默认 png

**示例**:

```json
{
    "name": "take_screenshot",
    "arguments": {
        "format": "png"
    }
}
```

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 1920,
    "height": 1080,
    "created_at": "2026-02-05T02:25:30Z"
}
```

---

### 6.1 take_screenshot_window

捕获指定窗口的截图（通过窗口标题匹配）。

**风险等级**: High（高风险）

**参数**:

- `title` (string, 必需): 窗口标题（支持部分匹配，不区分大小写）

**示例**:

```json
{
    "name": "take_screenshot_window",
    "arguments": {
        "title": "Chrome"
    }
}
```

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 1024,
    "height": 768,
    "created_at": "2026-02-05T02:25:30Z"
}
```

**特性**:

- 支持精确匹配和模糊匹配（优先精确匹配）
- 不区分大小写
- 自动恢复最小化的窗口
- 自动将窗口置于前台
- 使用 PrintWindow API 捕获完整内容

**错误响应**:

```json
{
    "error": "Window not found"
}
```

---

### 6.2 take_screenshot_region

捕获屏幕指定区域的截图。

**风险等级**: High（高风险）

**参数**:

- `x` (number, 必需): 区域左上角 X 坐标（像素）
- `y` (number, 必需): 区域左上角 Y 坐标（像素）
- `width` (number, 必需): 区域宽度（像素）
- `height` (number, 必需): 区域高度（像素）

**坐标系统**:

- 原点 (0, 0) 在屏幕左上角
- X 轴向右增加，Y 轴向下增加

**示例**:

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

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 800,
    "height": 600,
    "created_at": "2026-02-05T02:25:30Z"
}
```

**使用场景**:

- 捕获屏幕特定区域
- 裁剪截图
- 精确控制截图范围

**错误响应**:

```json
{
    "error": "Invalid region size"
}
```

### 7. list_windows

列出所有打开的窗口。

**参数**: 无

**示例**:

```json
{
    "name": "list_windows",
    "arguments": {}
}
```

### 7.1 focus_window

按窗口标题或进程名聚焦窗口。

**参数**:

- `title` (string, 可选): 窗口标题（支持部分匹配）
- `process_name` (string, 可选): 进程名（如 notepad.exe）

**示例**:

```json
{
    "name": "focus_window",
    "arguments": {
        "title": "Notepad"
    }
}
```

### 7.2 set_window_topmost

设置窗口置顶或取消置顶。

**参数**:

- `title` (string, 可选)
- `process_name` (string, 可选)
- `pid` (number, 可选)
- `topmost` (boolean, 可选，默认 true)

**示例**:

```json
{
    "name": "set_window_topmost",
    "arguments": {
        "process_name": "notepad.exe",
        "topmost": true
    }
}
```

### 7.3 set_window_state

设置窗口状态：最小化/最大化/恢复。

**参数**:

- `action` (string, 必需): minimize / maximize / restore
- `title` (string, 可选)
- `process_name` (string, 可选)
- `pid` (number, 可选)

**示例**:

```json
{
    "name": "set_window_state",
    "arguments": {
        "title": "Notepad",
        "action": "minimize"
    }
}
```

### 7.4 send_hotkey

发送快捷键组合，可选指定窗口（若指定会先聚焦）。

**参数**:

- `hotkey` (string, 必需): 如 "ctrl+shift+esc" / "win+alt+s"
- `title` (string, 可选)
- `process_name` (string, 可选)

**示例**:

```json
{
    "name": "send_hotkey",
    "arguments": {
        "hotkey": "ctrl+shift+esc",
        "title": "Notepad"
    }
}
```

### 8. list_processes

列出所有运行中的进程。

**参数**: 无

**示例**:

```json
{
    "name": "list_processes",
    "arguments": {}
}
```

### 9. execute_command

执行系统命令。

**参数**:

- `command` (string, 必需): 要执行的命令

**示例**:

```json
{
    "name": "execute_command",
    "arguments": {
        "command": "dir C:\\"
    }
}
```

### 9.1 search_files

基于文件名、元数据以及可选的内容搜索文件。

**参数**:

- `path` (string, 可选): 指定搜索路径（不传则在 allowed_dirs 中搜索）
- `name_query` (string, 可选): 文件名包含
- `content_query` (string, 可选): 文本内容包含
- `exts` (array, 可选): 后缀过滤
- `days` (number, 可选): 最近 N 天
- `min_size` / `max_size` (number, 可选): 字节
- `max` (number, 可选): 最大返回数量（默认 100）

**示例**:

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

---

## v0.3.0 新增工具 - 8 个工具

### 进程管理工具 (2 个)

#### 10. kill_process

终止指定 PID 的进程。

**风险等级**: High（高风险）

**参数**:

- `pid` (number, 必需): 进程 ID
- `force` (boolean, 可选): 是否强制终止，默认 false

**示例**:

```json
{
    "name": "kill_process",
    "arguments": {
        "pid": 1234,
        "force": false
    }
}
```

**响应**:

```json
{
    "success": true,
    "pid": 1234,
    "process_name": "notepad.exe",
    "forced": false
}
```

**安全注意事项**:

- 受保护的系统进程无法被终止（system, csrss.exe, winlogon.exe, services.exe, lsass.exe, smss.exe, wininit.exe）
- 高风险操作，需要用户确认
- 所有操作都会被记录到审计日志

---

#### 11. set_process_priority

调整进程优先级。

**风险等级**: Medium（中等风险）

**参数**:

- `pid` (number, 必需): 进程 ID
- `priority` (string, 必需): 优先级（idle, below_normal, normal, above_normal, high, realtime）

**示例**:

```json
{
    "name": "set_process_priority",
    "arguments": {
        "pid": 1234,
        "priority": "high"
    }
}
```

**响应**:

```json
{
    "success": true,
    "pid": 1234,
    "old_priority": "normal",
    "new_priority": "high"
}
```

**安全注意事项**:

- 设置 realtime 优先级需要管理员权限
- 不当的优先级设置可能影响系统性能

---

### 文件系统操作工具 (4 个)

#### 12. delete_file

删除文件或目录。

**风险等级**: High（高风险）

**参数**:

- `path` (string, 必需): 文件或目录路径
- `recursive` (boolean, 可选): 是否递归删除目录，默认 false

**示例**:

```json
{
    "name": "delete_file",
    "arguments": {
        "path": "C:\\Users\\test\\temp\\file.txt",
        "recursive": false
    }
}
```

**响应**:

```json
{
    "success": true,
    "path": "C:\\Users\\test\\temp\\file.txt",
    "type": "file"
}
```

**安全注意事项**:

- 路径必须在 allowed_dirs 白名单中
- 系统目录受保护（C:\Windows, C:\Program Files 等）
- 高风险操作，需要用户确认
- 删除操作不可恢复，请谨慎使用

---

#### 13. copy_file

复制文件或目录。

**风险等级**: Medium（中等风险）

**参数**:

- `source` (string, 必需): 源路径
- `destination` (string, 必需): 目标路径
- `overwrite` (boolean, 可选): 是否覆盖已存在的文件，默认 false

**示例**:

```json
{
    "name": "copy_file",
    "arguments": {
        "source": "C:\\Users\\test\\file.txt",
        "destination": "C:\\Users\\test\\backup\\file.txt",
        "overwrite": false
    }
}
```

**响应**:

```json
{
    "success": true,
    "source": "C:\\Users\\test\\file.txt",
    "destination": "C:\\Users\\test\\backup\\file.txt",
    "size": 1024
}
```

**安全注意事项**:

- 源路径和目标路径都必须在 allowed_dirs 白名单中
- 复制大文件可能需要较长时间

---

#### 14. move_file

移动或重命名文件。

**风险等级**: Medium（中等风险）

**参数**:

- `source` (string, 必需): 源路径
- `destination` (string, 必需): 目标路径

**示例**:

```json
{
    "name": "move_file",
    "arguments": {
        "source": "C:\\Users\\test\\old.txt",
        "destination": "C:\\Users\\test\\new.txt"
    }
}
```

**响应**:

```json
{
    "success": true,
    "source": "C:\\Users\\test\\old.txt",
    "destination": "C:\\Users\\test\\new.txt"
}
```

**安全注意事项**:

- 源路径和目标路径都必须在 allowed_dirs 白名单中
- 移动操作会删除源文件

---

#### 15. create_directory

创建新目录。

**风险等级**: Low（低风险）

**参数**:

- `path` (string, 必需): 目录路径
- `recursive` (boolean, 可选): 是否创建多级目录，默认 false

**示例**:

```json
{
    "name": "create_directory",
    "arguments": {
        "path": "C:\\Users\\test\\new_folder",
        "recursive": false
    }
}
```

**响应**:

```json
{
    "success": true,
    "path": "C:\\Users\\test\\new_folder"
}
```

**安全注意事项**:

- 路径必须在 allowed_dirs 白名单中

---

### 电源管理工具 (2 个)

#### 16. shutdown_system

执行系统关机、重启、休眠或睡眠。

**风险等级**: Critical（关键风险）

**参数**:

- `action` (string, 必需): 电源操作（shutdown, reboot, hibernate, sleep）
- `delay` (number, 可选): 延迟时间（秒），默认 0
- `force` (boolean, 可选): 是否强制关闭应用程序，默认 false
- `message` (string, 可选): 显示给用户的消息

**示例**:

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown",
        "delay": 60,
        "force": false,
        "message": "System will shutdown in 1 minute"
    }
}
```

**响应**:

```json
{
    "success": true,
    "action": "shutdown",
    "delay": 60,
    "scheduled_time": "2026-02-03T12:11:00.000Z"
}
```

**安全注意事项**:

- 关键风险操作，需要用户确认
- 需要 SE_SHUTDOWN_NAME 权限
- 建议以管理员身份运行
- 使用 delay 参数可以给用户取消的机会

---

#### 17. abort_shutdown

取消计划的关机或重启。

**风险等级**: Medium（中等风险）

**参数**: 无

**示例**:

```json
{
    "name": "abort_shutdown",
    "arguments": {}
}
```

**响应**:

```json
{
    "success": true,
    "message": "Shutdown cancelled successfully"
}
```

**安全注意事项**:

- 需要 SE_SHUTDOWN_NAME 权限
- 只能取消通过 shutdown_system 工具计划的关机

---

当工具调用失败时，响应中的 `isError` 字段将为 `true`，并在 `content` 中包含错误信息。

**错误响应示例**:

```json
{
    "content": [
        {
            "type": "text",
            "text": "Error: File not found"
        }
    ],
    "isError": true
}
```

## 使用示例

**重要：所有示例都需要添加 Authorization 头！**

### curl 示例

```bash
# 设置 Token（从 config.json 获取）
TOKEN="your-auth-token-from-config-json"

# 初始化
curl -X POST http://localhost:35182/mcp/initialize \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}'

# 列出工具
curl -X POST http://localhost:35182/mcp/tools/list \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{}'

# 调用工具
curl -X POST http://localhost:35182/mcp/tools/call \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"list_windows","arguments":{}}'
```

### Python 示例

```python
import requests
import json

SERVER = "http://localhost:35182"
TOKEN = "your-auth-token-from-config-json"  # 从 config.json 获取

# 设置认证头
headers = {
    "Authorization": f"Bearer {TOKEN}",
    "Content-Type": "application/json"
}

# 初始化
init_response = requests.post(
    f"{SERVER}/mcp/initialize",
    headers=headers,
    json={
        "protocolVersion": "2024-11-05",
        "capabilities": {},
        "clientInfo": {"name": "python-client", "version": "1.0"}
    }
)
print("Initialize:", init_response.json())

# 列出工具
tools_response = requests.post(
    f"{SERVER}/mcp/tools/list",
    headers=headers,
    json={}
)
print("Tools:", tools_response.json())

# 调用工具
call_response = requests.post(
    f"{SERVER}/mcp/tools/call",
    headers=headers,
    json={
        "name": "list_windows",
        "arguments": {}
    }
)
print("Result:", call_response.json())
```

### JavaScript 示例

```javascript
const SERVER = "http://localhost:35182";
const TOKEN = "your-auth-token-from-config-json"; // 从 config.json 获取

// 设置认证头
const headers = {
    Authorization: `Bearer ${TOKEN}`,
    "Content-Type": "application/json",
};

// 初始化
fetch(`${SERVER}/mcp/initialize`, {
    method: "POST",
    headers: headers,
    body: JSON.stringify({
        protocolVersion: "2024-11-05",
        capabilities: {},
        clientInfo: { name: "js-client", version: "1.0" },
    }),
})
    .then((res) => res.json())
    .then((data) => console.log("Initialize:", data));

// 列出工具
fetch(`${SERVER}/mcp/tools/list`, {
    method: "POST",
    headers: headers,
    body: JSON.stringify({}),
})
    .then((res) => res.json())
    .then((data) => console.log("Tools:", data));

// 调用工具
fetch(`${SERVER}/mcp/tools/call`, {
    method: "POST",
    headers: headers,
    body: JSON.stringify({
        name: "list_windows",
        arguments: {},
    }),
})
    .then((res) => res.json())
    .then((data) => console.log("Result:", data));
```

## 与 Claude Desktop 集成

**重要：需要配置 Token 认证！**

### 配置方法

由于 ClawDesk Server 需要 Bearer Token 认证，你需要创建一个 MCP 客户端脚本来桥接 Claude Desktop 和 ClawDesk Server。

**步骤 1：获取 Token**

从 ClawDesk Server 的 `config.json` 文件中获取 `auth_token`。

**步骤 2：创建 MCP 客户端**

创建 `clawdesk-mcp-client.js` 文件：

```javascript
// clawdesk-mcp-client.js
const fetch = require("node-fetch");

const SERVER_URL = process.env.CLAWDESK_URL || "http://localhost:35182";
const AUTH_TOKEN = process.env.CLAWDESK_TOKEN;

if (!AUTH_TOKEN) {
    console.error("Error: CLAWDESK_TOKEN environment variable not set");
    process.exit(1);
}

const headers = {
    Authorization: `Bearer ${AUTH_TOKEN}`,
    "Content-Type": "application/json",
};

// 实现 MCP 客户端逻辑
// 处理 stdin/stdout 通信
// 转发请求到 ClawDesk Server
// ...
```

**步骤 3：配置 Claude Desktop**

编辑 Claude Desktop 配置文件：

- macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
- Windows: `%APPDATA%\Claude\claude_desktop_config.json`

添加配置：

```json
{
    "mcpServers": {
        "clawdesk": {
            "command": "node",
            "args": ["/path/to/clawdesk-mcp-client.js"],
            "env": {
                "CLAWDESK_URL": "http://localhost:35182",
                "CLAWDESK_TOKEN": "your-auth-token-from-config-json"
            }
        }
    }
}
```

### 使用方法

1. 启动 ClawDesk MCP Server
2. 从 `config.json` 获取 `auth_token`
3. 在 Claude Desktop 配置中设置 `CLAWDESK_TOKEN`
4. 启动 Claude Desktop
5. Claude 将通过客户端连接到服务器

**示例对话**:

```
User: 请列出当前打开的所有窗口

Claude: 我来帮你查看当前打开的窗口...
[调用 list_windows 工具]
当前打开的窗口有：
1. Google Chrome - ...
2. Visual Studio Code - ...
...
```

## 安全注意事项

1. **Token 认证**: v0.2.0 开始所有请求都需要 Bearer Token
2. **Token 安全**: 不要将 Token 提交到版本控制或公开分享
3. **网络访问**: 可配置监听 0.0.0.0（网络）或 127.0.0.1（本地）
4. **命令执行**: execute_command 工具具有高风险，请谨慎使用
5. **文件访问**: 建议配置 allowed_dirs 白名单限制访问范围
6. **防火墙**: 网络访问时建议配置防火墙规则

## 限制和已知问题

1. **简化实现**: 当前为基础实现，未完全遵循 MCP 规范
2. **工具参数**: 部分工具的参数解析较为简单
3. **错误处理**: 错误信息可能不够详细
4. **流式响应**: 不支持流式响应
5. **资源管理**: 不支持资源（resources）功能
6. **提示词**: 不支持提示词（prompts）功能

## 未来计划

- [ ] 完整的 MCP 规范实现
- [x] Bearer Token 认证（v0.2.0 已实现）
- [ ] 资源（resources）支持
- [ ] 提示词（prompts）支持
- [ ] 流式响应
- [ ] 更详细的错误信息
- [ ] 工具调用日志和审计
- [ ] 用户确认机制

## 参考资料

- [Model Context Protocol 规范](https://modelcontextprotocol.io/)
- [MCP GitHub](https://github.com/modelcontextprotocol)
- [Claude Desktop MCP 文档](https://docs.anthropic.com/claude/docs/mcp)

## 版本历史

- v0.3.0 (2026-02-04)
    - ✅ 8 个新工具（进程管理、文件操作、电源管理）
    - ✅ 高风险操作审计和确认机制
    - ✅ 受保护进程和系统目录保护
    - ✅ 总计 17 个工具

- v0.2.0 (2026-02-03)
    - ✅ 基础 MCP 协议实现
    - ✅ 9 个工具支持
    - ✅ initialize、tools/list、tools/call 端点
