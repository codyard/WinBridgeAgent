# ClawDesk MCP Server - API 文档

## 概述

ClawDesk MCP Server 提供基于 HTTP 的 RESTful API，用于远程控制 Windows 系统操作。

**基础 URL**: `http://localhost:35182` (默认端口)

**网络访问**: `http://192.168.x.x:35182` (需要 listen_address 设置为 "0.0.0.0")

## 通用响应格式

所有 API 响应都使用 JSON 格式，并包含 CORS 头以支持跨域访问。

### 成功响应

```json
{
  "status": "ok",
  "data": { ... }
}
```

### 错误响应

```json
{
    "error": "错误描述"
}
```

## API 端点

### 1. 获取 API 列表

获取所有可用的 API 端点列表和 MCP 工具列表。

**请求**:

```
GET /
GET /help  (别名)
```

**响应**:

```json
{
  "name": "ClawDesk MCP Server",
  "version": "0.4.0",
  "status": "running",
  "endpoints": [
    {
      "path": "/",
      "method": "GET",
      "description": "API endpoint list"
    },
    {
      "path": "/help",
      "method": "GET",
      "description": "API endpoint list"
    },
    ...
  ],
  "mcp_tools": [
    {
      "name": "take_screenshot",
      "description": "Take a full screen screenshot"
    },
    {
      "name": "take_screenshot_window",
      "description": "Take screenshot of a window by title"
    },
    {
      "name": "take_screenshot_region",
      "description": "Take screenshot of a region"
    },
    ...
  ]
}
```

**字段说明**:

- `endpoints`: HTTP API 端点列表
- `mcp_tools`: MCP 工具列表（通过 MCP 协议调用）

---

### 2. 健康检查

快速检查服务器是否运行正常。

**请求**:

```
GET /health
```

**响应**:

```json
{
    "status": "ok"
}
```

---

### 3. 获取服务器状态

获取服务器的详细状态信息。

**请求**:

```
GET /status
GET /sts  (别名)
```

**响应**:

```json
{
    "status": "running",
    "version": "0.2.0",
    "port": 35182,
    "listen_address": "0.0.0.0",
    "local_ip": "192.168.31.3",
    "license": "free",
    "uptime_seconds": 1234,
    "endpoints": ["/sts", "/status", "/health", "/exit"]
}
```

**字段说明**:

- `status`: 服务器状态 (running)
- `version`: 服务器版本号
- `port`: 监听端口
- `listen_address`: 监听地址 (0.0.0.0 或 127.0.0.1)
- `local_ip`: 本机 IP 地址
- `license`: 许可证类型 (free 或 professional)
- `uptime_seconds`: 运行时间（秒）

---

### 4. 获取磁盘列表

列出所有磁盘驱动器及其详细信息。

**请求**:

```
GET /disks
```

**响应**:

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
    },
    {
        "drive": "D:",
        "type": "removable",
        "label": "USB Drive",
        "filesystem": "FAT32",
        "total_bytes": 32000000000,
        "free_bytes": 16000000000,
        "used_bytes": 16000000000
    }
]
```

**字段说明**:

- `drive`: 驱动器盘符
- `type`: 驱动器类型
    - `fixed`: 固定磁盘
    - `removable`: 可移动磁盘
    - `network`: 网络驱动器
    - `cdrom`: 光驱
    - `ramdisk`: 内存盘
- `label`: 卷标
- `filesystem`: 文件系统类型 (NTFS, FAT32, exFAT 等)
- `total_bytes`: 总容量（字节）
- `free_bytes`: 可用空间（字节）
- `used_bytes`: 已用空间（字节）

---

### 5. 列出目录内容

列出指定目录下的所有文件和子目录。

**请求**:

```
GET /list?path=<目录路径>
```

**参数**:

- `path` (必需): 目录路径，支持 URL 编码

**示例**:

```bash
# Windows 路径
GET /list?path=C:\Users
GET /list?path=C:\Users\Documents

# URL 编码路径
GET /list?path=C%3A%5CUsers
```

**响应**:

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

**字段说明**:

- `name`: 文件或目录名
- `type`: 类型 (file 或 directory)
- `size`: 文件大小（字节），目录为 0
- `modified`: 最后修改时间

**错误响应**:

```json
{
    "error": "File not found or access denied"
}
```

---

### 6. 读取文件内容

读取文件内容，支持行范围、尾部读取和行数统计。

**请求**:

```
GET /read?path=<文件路径>&start=<起始行>&lines=<行数>&tail=<尾部行数>&count=<是否只统计>
```

**参数**:

- `path` (必需): 文件路径
- `start` (可选): 起始行号，默认 0
- `lines` (可选): 读取行数，默认全部
- `tail` (可选): 从尾部读取 n 行
- `count` (可选): 只返回行数，不返回内容 (true/1)

**示例**:

```bash
# 读取整个文件
GET /read?path=C:\test.txt

# 从第 10 行开始读取 20 行
GET /read?path=C:\test.txt&start=10&lines=20

# 读取最后 50 行
GET /read?path=C:\test.txt&tail=50

# 只获取行数
GET /read?path=C:\test.txt&count=true
```

**响应（完整内容）**:

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

**响应（仅行数）**:

```json
{
    "path": "C:\\test.txt",
    "total_lines": 100,
    "file_size": 5120
}
```

**限制**:

- 最大文件大小: 10MB
- 超过限制返回 413 错误

**错误响应**:

```json
{
    "error": "File not found or access denied"
}
```

```json
{
    "error": "File too large",
    "max_size": "10MB"
}
```

---

### 7. 搜索文件内容

在文件中搜索指定关键词。

**请求**:

```
GET /search?path=<文件路径>&query=<搜索词>&case=<大小写>&max=<最大结果数>
```

**参数**:

- `path` (必需): 文件路径
- `query` (必需): 搜索关键词
- `case` (可选): 大小写敏感性
    - `sensitive` (默认): 区分大小写
    - `i` 或 `insensitive`: 不区分大小写
- `max` (可选): 最大结果数，默认 100，最大 1000

**示例**:

```bash
# 区分大小写搜索
GET /search?path=C:\test.txt&query=keyword

# 不区分大小写搜索
GET /search?path=C:\test.txt&query=keyword&case=i

# 限制结果数量
GET /search?path=C:\test.txt&query=keyword&max=50
```

**响应**:

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
        },
        {
            "line_number": 25,
            "content": "Another line with keyword"
        }
    ]
}
```

**字段说明**:

- `path`: 文件路径
- `query`: 搜索关键词
- `total_lines`: 文件总行数
- `match_count`: 匹配数量
- `case_sensitive`: 是否区分大小写
- `matches`: 匹配结果数组
    - `line_number`: 行号（从 0 开始）
    - `content`: 行内容

**限制**:

- 最大文件大小: 10MB
- 最大结果数: 1000

**错误响应**:

```json
{
    "error": "Missing required parameter: query"
}
```

---

### 8. 读取剪贴板

获取当前剪贴板的内容。

**增强功能**：支持文本、图片和文件三种类型！

**请求**:

```
GET /clipboard
```

**响应类型 1 - 文本**:

```json
{
    "type": "text",
    "content": "剪贴板文本内容",
    "length": 15,
    "empty": false
}
```

**响应类型 2 - 图片**（截图或复制的图片）:

```json
{
    "type": "image",
    "format": "png",
    "url": "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png",
    "path": "/clipboard/image/clipboard_images/clipboard_20260203_223015.png"
}
```

**响应类型 3 - 文件**（从资源管理器复制的文件）:

```json
{
    "type": "files",
    "files": [
        {
            "name": "20260203_223015_0_document.pdf",
            "url": "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf",
            "path": "/clipboard/file/20260203_223015_0_document.pdf"
        },
        {
            "name": "20260203_223015_1_image.jpg",
            "url": "http://192.168.31.3:35182/clipboard/file/20260203_223015_1_image.jpg",
            "path": "/clipboard/file/20260203_223015_1_image.jpg"
        }
    ]
}
```

**字段说明**:

- `type`: 剪贴板内容类型（"text", "image", "files"）
- `content`: 文本内容（仅 type="text"）
- `length`: 文本长度（仅 type="text"）
- `empty`: 是否为空（仅 type="text"）
- `format`: 图片格式（仅 type="image"）
- `url`: 完整访问 URL
- `path`: 相对路径
- `files`: 文件列表（仅 type="files"）
- `name`: 文件名

**下载图片或文件**:

```bash
# 下载剪贴板图片
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png" \
  -o clipboard_image.png

# 下载剪贴板文件
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf" \
  -o document.pdf
```

**使用场景**:

1. **获取截图**: 用户按 Win+Shift+S 截图后，调用 API 获取图片
2. **获取复制的文件**: 用户在资源管理器中复制文件后，调用 API 获取文件列表
3. **获取文本**: 用户复制文本后，调用 API 获取文本内容

**错误响应**:

```json
{
    "error": "Failed to open clipboard"
}
```

---

### 9. 写入剪贴板

设置剪贴板的文本内容（仅支持文本）。

**请求**:

```
PUT /clipboard
Content-Type: application/json

{
  "content": "要写入的内容"
}
```

**请求体**:

```json
{
    "content": "要写入的内容"
}
```

**字段说明**:

- `content` (必需): 要写入剪贴板的文本内容

**响应**:

```json
{
    "success": true,
    "length": 18
}
```

**字段说明**:

- `success`: 是否成功
- `length`: 写入的内容长度

**示例**:

```bash
# 写入简单文本
curl -X PUT \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"content":"Hello World"}' \
  http://localhost:35182/clipboard

# 写入多行文本
curl -X PUT \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"content":"Line 1\nLine 2\nLine 3"}' \
  http://localhost:35182/clipboard

# 写入特殊字符
curl -X PUT \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"content":"Special: \"quotes\", \ttabs, and \\backslashes\\"}' \
  http://localhost:35182/clipboard
```

**错误响应**:

```json
{
    "error": "Missing request body"
}
```

```json
{
    "error": "Missing 'content' field in JSON"
}
```

```json
{
    "error": "Failed to open clipboard"
}
```

---

### 10. 截图

捕获屏幕截图并返回图片 URL。

**功能增强**: 现在支持全屏、指定窗口和指定区域截图！

#### 10.1 全屏截图

**请求**:

```
GET /screenshot?format=<格式>
POST /screenshot
```

**参数**:

- `format` (可选): 图像格式
    - `png` (默认): PNG 格式
    - `jpg` 或 `jpeg`: JPEG 格式

**示例**:

```bash
# PNG 格式（默认）
GET /screenshot

# JPEG 格式
GET /screenshot?format=jpg
```

**响应**:

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

**字段说明**:

- `success`: 是否成功
- `format`: 图像格式 (png 或 jpg)
- `width`: 屏幕宽度（像素）
- `height`: 屏幕高度（像素）
- `url`: 完整访问 URL
- `path`: 相对路径

#### 10.2 下载截图文件

**请求**:

```
GET /screenshot/file/<文件名>
```

**示例**:

```bash
# 下载截图
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/screenshot/file/screenshot_20260205_102530.png" \
  -o screenshot.png
```

**响应**: 返回图片文件的二进制数据

---

## MCP 工具 - 高级截图功能

以下功能通过 MCP 协议调用，提供更强大的截图能力。

### 10.3 指定窗口截图

通过窗口标题捕获特定窗口的截图。

**MCP 工具**: `take_screenshot_window`

**参数**:

```json
{
    "title": "窗口标题"
}
```

**参数说明**:

- `title` (string, 必需): 窗口标题（支持部分匹配）
    - 优先精确匹配
    - 如果没有精确匹配，则使用部分匹配
    - 不区分大小写

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 1024,
    "height": 768,
    "created_at": "2026-02-05T02:25:30Z"
}
```

**字段说明**:

- `path`: 截图文件路径
- `width`: 窗口宽度（像素）
- `height`: 窗口高度（像素）
- `created_at`: 创建时间（ISO 8601 格式）

**使用示例**:

```json
// 捕获记事本窗口
{
    "title": "notepad"
}

// 捕获 Chrome 浏览器窗口
{
    "title": "Chrome"
}

// 捕获特定文档窗口
{
    "title": "Document.docx - Microsoft Word"
}
```

**特性**:

- 自动恢复最小化的窗口
- 自动将窗口置于前台
- 使用 PrintWindow API 捕获完整内容
- 支持模糊匹配窗口标题

**错误响应**:

```json
{
    "error": "Window not found"
}
```

```json
{
    "error": "Title is required"
}
```

---

### 10.4 指定区域截图

捕获屏幕上指定矩形区域的截图。

**MCP 工具**: `take_screenshot_region`

**参数**:

```json
{
    "x": 100,
    "y": 100,
    "width": 800,
    "height": 600
}
```

**参数说明**:

- `x` (number, 必需): 区域左上角 X 坐标（像素）
- `y` (number, 必需): 区域左上角 Y 坐标（像素）
- `width` (number, 必需): 区域宽度（像素）
- `height` (number, 必需): 区域高度（像素）

**坐标系统**:

- 原点 (0, 0) 在屏幕左上角
- X 轴向右增加
- Y 轴向下增加

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 800,
    "height": 600,
    "created_at": "2026-02-05T02:25:30Z"
}
```

**使用示例**:

```json
// 捕获屏幕左上角 800x600 区域
{
    "x": 0,
    "y": 0,
    "width": 800,
    "height": 600
}

// 捕获屏幕中心 1024x768 区域（假设屏幕为 1920x1080）
{
    "x": 448,
    "y": 156,
    "width": 1024,
    "height": 768
}

// 捕获屏幕右下角 400x300 区域（假设屏幕为 1920x1080）
{
    "x": 1520,
    "y": 780,
    "width": 400,
    "height": 300
}
```

**错误响应**:

```json
{
    "error": "Invalid region size"
}
```

---

### 截图功能对比

| 功能         | HTTP API (/screenshot) | MCP: take_screenshot_window | MCP: take_screenshot_region |
| ------------ | ---------------------- | --------------------------- | --------------------------- |
| 全屏截图     | ✅                     | ❌                          | ❌                          |
| 窗口截图     | ❌                     | ✅                          | ❌                          |
| 区域截图     | ❌                     | ❌                          | ✅                          |
| 格式选择     | ✅ (png/jpg)           | ✅ (png)                    | ✅ (png)                    |
| 返回 URL     | ✅                     | ❌                          | ❌                          |
| 返回文件路径 | ✅                     | ✅                          | ✅                          |
| 窗口匹配     | ❌                     | ✅ (精确+模糊)              | ❌                          |
| 坐标控制     | ❌                     | ❌                          | ✅                          |

---

### 使用场景

1. **全屏截图**: 快速捕获整个屏幕，适合监控和记录
2. **窗口截图**: 捕获特定应用程序窗口，适合文档和演示
3. **区域截图**: 精确捕获屏幕特定区域，适合裁剪和局部截图

---

### 使用示例

#### curl 示例

```bash
# 全屏截图（PNG）
curl http://localhost:35182/screenshot

# 全屏截图（JPEG）
curl "http://localhost:35182/screenshot?format=jpg"

# 下载截图文件
curl "http://localhost:35182/screenshot/file/screenshot_20260205_102530.png" -o screenshot.png
```

#### MCP 调用示例

```json
// 窗口截图
POST /mcp/tools/call
{
  "name": "take_screenshot_window",
  "arguments": {
    "title": "Chrome"
  }
}

// 区域截图
POST /mcp/tools/call
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

**错误响应**:

```json
{
    "error": "Failed to capture screenshot"
}
```

---

### 11. 退出服务器

优雅地关闭服务器。

**请求**:

```
GET /exit
```

**响应**:

```json
{
    "status": "shutting down"
}
```

**说明**:

- 服务器会先关闭 Dashboard 窗口
- 然后关闭主窗口和托盘图标
- 最后停止 HTTP 服务器线程

---

## CORS 支持

所有 API 端点都支持 CORS（跨域资源共享），允许从浏览器直接调用。

**CORS 头**:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

**OPTIONS 预检请求**:

```
OPTIONS /list?path=C:\
```

响应:

```
HTTP/1.1 200 OK
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

---

## 错误代码

| HTTP 状态码 | 说明           |
| ----------- | -------------- |
| 200         | 成功           |
| 400         | 请求参数错误   |
| 404         | 资源未找到     |
| 413         | 文件过大       |
| 500         | 服务器内部错误 |

---

## 使用示例

### curl 示例

```bash
# 获取状态
curl http://localhost:35182/status

# 列出目录
curl "http://localhost:35182/list?path=C:\\"

# 读取文件
curl "http://localhost:35182/read?path=C:\\test.txt&tail=10"

# 搜索文件
curl "http://localhost:35182/search?path=C:\\test.txt&query=error&case=i"

# 读取剪贴板
curl http://localhost:35182/clipboard

# 写入剪贴板
curl -X PUT -H "Content-Type: application/json" \
  -d '{"content":"Hello from API"}' \
  http://localhost:35182/clipboard

# 截图（PNG）
curl http://localhost:35182/screenshot | jq -r '.data' | base64 -d > screenshot.png

# 截图（JPEG）
curl "http://localhost:35182/screenshot?format=jpg" | jq -r '.data' | base64 -d > screenshot.jpg
```

### PowerShell 示例

```powershell
# 获取状态
Invoke-RestMethod -Uri "http://localhost:35182/status"

# 列出目录
Invoke-RestMethod -Uri "http://localhost:35182/list?path=C:\"

# 读取文件
Invoke-RestMethod -Uri "http://localhost:35182/read?path=C:\test.txt"

# 读取剪贴板
Invoke-RestMethod -Uri "http://localhost:35182/clipboard"

# 写入剪贴板
$body = @{ content = "Hello World" } | ConvertTo-Json
Invoke-RestMethod -Uri "http://localhost:35182/clipboard" `
  -Method PUT -ContentType "application/json" -Body $body

# 截图
$response = Invoke-RestMethod -Uri "http://localhost:35182/screenshot"
[System.Convert]::FromBase64String($response.data) | Set-Content -Path "screenshot.png" -Encoding Byte
```

### JavaScript 示例

```javascript
// 获取状态
fetch("http://localhost:35182/status")
    .then((response) => response.json())
    .then((data) => console.log(data));

// 读取剪贴板
fetch("http://localhost:35182/clipboard")
    .then((response) => response.json())
    .then((data) => console.log("Clipboard:", data.content));

// 写入剪贴板
fetch("http://localhost:35182/clipboard", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ content: "Hello from JavaScript" }),
})
    .then((response) => response.json())
    .then((data) => console.log("Success:", data));

// 截图并显示
fetch("http://localhost:35182/screenshot")
    .then((response) => response.json())
    .then((data) => {
        const img = document.createElement("img");
        img.src = `data:image/${data.format};base64,${data.data}`;
        document.body.appendChild(img);
    });
```

---

## 安全注意事项

1. **认证**: 当前版本未实现认证机制，建议仅在可信网络中使用
2. **监听地址**:
    - `127.0.0.1`: 仅本地访问，最安全
    - `0.0.0.0`: 允许网络访问，需要防火墙保护
3. **文件访问**: 建议在配置文件中设置 `allowed_dirs` 白名单
4. **防火墙**: 使用 `0.0.0.0` 时需要添加 Windows 防火墙规则

---

---

## v0.3.0 新增端点

### 12. 终止进程

终止指定 PID 的进程。

**请求**:

```
POST /process/kill
Content-Type: application/json

{
  "pid": 1234,
  "force": false
}
```

**参数**:

- `pid` (number, 必需): 进程 ID
- `force` (boolean, 可选): 是否强制终止，默认 false
    - `false`: 先尝试发送 WM_CLOSE 消息，失败后使用 TerminateProcess
    - `true`: 直接使用 TerminateProcess 强制终止

**响应**:

```json
{
    "success": true,
    "pid": 1234,
    "process_name": "notepad.exe",
    "forced": false
}
```

**受保护进程**:

以下系统进程无法被终止（硬编码保护）：

- system
- csrss.exe
- winlogon.exe
- services.exe
- lsass.exe
- smss.exe
- wininit.exe

**错误响应**:

```json
{
    "error": "Process is protected and cannot be terminated"
}
```

```json
{
    "error": "Process not found or access denied"
}
```

---

### 13. 调整进程优先级

调整指定进程的优先级。

**请求**:

```
POST /process/priority
Content-Type: application/json

{
  "pid": 1234,
  "priority": "normal"
}
```

**参数**:

- `pid` (number, 必需): 进程 ID
- `priority` (string, 必需): 优先级
    - `idle`: 空闲
    - `below_normal`: 低于正常
    - `normal`: 正常
    - `above_normal`: 高于正常
    - `high`: 高
    - `realtime`: 实时（需要管理员权限）

**响应**:

```json
{
    "success": true,
    "pid": 1234,
    "old_priority": "normal",
    "new_priority": "high"
}
```

**错误响应**:

```json
{
    "error": "Insufficient privileges for realtime priority"
}
```

---

### 14. 删除文件或目录

删除指定的文件或目录。

**请求**:

```
DELETE /file
Content-Type: application/json

{
  "path": "C:\\Users\\test\\temp\\file.txt",
  "recursive": false
}
```

**参数**:

- `path` (string, 必需): 文件或目录路径
- `recursive` (boolean, 可选): 是否递归删除目录，默认 false

**响应**:

```json
{
    "success": true,
    "path": "C:\\Users\\test\\temp\\file.txt",
    "type": "file"
}
```

**安全限制**:

1. **白名单检查**: 路径必须在 `allowed_dirs` 白名单中
2. **系统目录保护**: 以下目录无法删除（硬编码保护）：
    - C:\Windows
    - C:\Program Files
    - C:\Program Files (x86)
    - C:\ProgramData\Microsoft

**错误响应**:

```json
{
    "error": "Path not allowed"
}
```

```json
{
    "error": "System directory protected"
}
```

```json
{
    "error": "Directory not empty (use recursive=true)"
}
```

---

### 15. 复制文件或目录

复制文件或目录到目标位置。

**请求**:

```
POST /file/copy
Content-Type: application/json

{
  "source": "C:\\Users\\test\\file.txt",
  "destination": "C:\\Users\\test\\backup\\file.txt",
  "overwrite": false
}
```

**参数**:

- `source` (string, 必需): 源路径
- `destination` (string, 必需): 目标路径
- `overwrite` (boolean, 可选): 是否覆盖已存在的文件，默认 false

**响应**:

```json
{
    "success": true,
    "source": "C:\\Users\\test\\file.txt",
    "destination": "C:\\Users\\test\\backup\\file.txt",
    "size": 1024
}
```

**安全限制**:

- 源路径和目标路径都必须在 `allowed_dirs` 白名单中

**错误响应**:

```json
{
    "error": "Destination file already exists (use overwrite=true)"
}
```

---

### 16. 移动或重命名文件

移动或重命名文件。

**请求**:

```
POST /file/move
Content-Type: application/json

{
  "source": "C:\\Users\\test\\old.txt",
  "destination": "C:\\Users\\test\\new.txt"
}
```

**参数**:

- `source` (string, 必需): 源路径
- `destination` (string, 必需): 目标路径

**响应**:

```json
{
    "success": true,
    "source": "C:\\Users\\test\\old.txt",
    "destination": "C:\\Users\\test\\new.txt"
}
```

**安全限制**:

- 源路径和目标路径都必须在 `allowed_dirs` 白名单中

---

### 17. 创建目录

创建新目录。

**请求**:

```
POST /directory
Content-Type: application/json

{
  "path": "C:\\Users\\test\\new_folder",
  "recursive": false
}
```

**参数**:

- `path` (string, 必需): 目录路径
- `recursive` (boolean, 可选): 是否创建多级目录，默认 false

**响应**:

```json
{
    "success": true,
    "path": "C:\\Users\\test\\new_folder"
}
```

**安全限制**:

- 路径必须在 `allowed_dirs` 白名单中

**错误响应**:

```json
{
    "error": "Parent directory does not exist (use recursive=true)"
}
```

---

### 18. 系统关机/重启/休眠/睡眠

执行电源管理操作。

**请求**:

```
POST /power/shutdown
Content-Type: application/json

{
  "action": "shutdown",
  "delay": 60,
  "force": false,
  "message": "System will shutdown in 1 minute"
}
```

**参数**:

- `action` (string, 必需): 电源操作
    - `shutdown`: 关机
    - `reboot`: 重启
    - `hibernate`: 休眠
    - `sleep`: 睡眠
- `delay` (number, 可选): 延迟时间（秒），默认 0
- `force` (boolean, 可选): 是否强制关闭应用程序，默认 false
- `message` (string, 可选): 显示给用户的消息

**响应**:

```json
{
    "success": true,
    "action": "shutdown",
    "delay": 60,
    "scheduled_time": "2026-02-03T12:11:00.000Z"
}
```

**权限要求**:

- 需要 SE_SHUTDOWN_NAME 权限
- 建议以管理员身份运行

**错误响应**:

```json
{
    "error": "Insufficient privileges"
}
```

---

### 19. 取消关机

取消计划的关机或重启。

**请求**:

```
POST /power/abort
```

**响应**:

```json
{
    "success": true,
    "message": "Shutdown cancelled successfully"
}
```

**错误响应**:

```json
{
    "error": "No shutdown is scheduled"
}
```

---

## 版本历史

- v0.3.0 (2026-02-04)
    - ✅ 进程管理（终止、优先级调整）
    - ✅ 文件系统操作（删除、复制、移动、创建目录）
    - ✅ 电源管理（关机、重启、休眠、睡眠）
    - ✅ 高风险操作审计
    - ✅ 受保护进程和系统目录保护

- v0.2.0 (2026-02-03)
    - ✅ 基础 HTTP API
    - ✅ 磁盘和文件操作
    - ✅ 剪贴板操作
    - ✅ CORS 支持
