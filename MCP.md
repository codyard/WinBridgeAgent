# WinBridgeAgent - MCP Protocol Documentation

[中文文档](MCP_zh.md)

## 概述

WinBridgeAgent 实现了 Model Context Protocol (MCP) 标准协议，使 AI 助手能够通过标准化的方式调用 Windows 系统功能。

**开源版本特性**：
- ✅ 无需认证，直接访问所有 API
- ✅ 完整的系统操作能力
- ✅ 实时审计日志
- ✅ 支持 HTTP 和 SSE 传输

## MCP 协议版本

- **协议版本**: 2024-11-05
- **实现状态**: 完整实现
- **发布日期**: 2026-02-06
- **支持的功能**: 工具调用、资源访问、提示

### 工具分类

| 分类 | 工具数量 | 工具列表 |
|------|----------|----------|
| 文件操作 | 8 | `read_file`, `write_file`, `list_directory`, `search_files`, `create_directory`, `delete_file`, `move_file`, `copy_file` |
| 系统信息 | 4 | `list_disks`, `get_system_info`, `list_processes`, `list_windows` |
| 剪贴板 | 2 | `read_clipboard`, `write_clipboard` |
| 截图 | 3 | `take_screenshot`, `take_region_screenshot`, `take_window_screenshot` |
| 电源管理 | 3 | `shutdown`, `restart`, `sleep` |
| 执行命令 | 2 | `execute_command`, `execute_powershell` |
| 浏览器 | 2 | `list_browser_tabs`, `close_browser_tab` |

**总计**: 24 个工具

## 连接配置

### 基本连接

```bash
# 初始化 MCP 会话
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}' \
  http://<windows-ip>:35182/mcp/initialize
```

### 列出可用工具

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{}' \
  http://<windows-ip>:35182/mcp/tools/list
```

### 调用工具示例

```bash
# 列出目录内容
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"name":"list_directory","arguments":{"path":"C:\\"}}' \
  http://<windows-ip>:35182/mcp/tools/call

# 读取文件
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"name":"read_file","arguments":{"path":"C:\\test.txt"}}' \
  http://<windows-ip>:35182/mcp/tools/call

# 执行命令
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"name":"execute_command","arguments":{"command":"dir"}}' \
  http://<windows-ip>:35182/mcp/tools/call
```

## 工具详细说明

### 文件操作工具

#### `read_file`
读取文件内容，支持行范围和尾部读取。

**参数**：
- `path` (string, 必需): 文件路径
- `start` (integer, 可选): 起始行号（从0开始）
- `lines` (integer, 可选): 读取行数
- `tail` (integer, 可选): 从文件末尾读取的行数

#### `write_file`
写入文件内容。

**参数**：
- `path` (string, 必需): 文件路径
- `content` (string, 必需): 文件内容
- `append` (boolean, 可选): 是否追加写入（默认false）

#### `list_directory`
列出目录内容。

**参数**：
- `path` (string, 必需): 目录路径
- `show_hidden` (boolean, 可选): 是否显示隐藏文件（默认false）

#### `search_files`
在文件中搜索内容。

**参数**：
- `path` (string, 必需): 文件路径
- `query` (string, 必需): 搜索关键词
- `case_sensitive` (boolean, 可选): 是否区分大小写（默认false）
- `max_results` (integer, 可选): 最大结果数（默认50）

### 系统信息工具

#### `list_disks`
获取所有磁盘驱动器信息。

**返回**：驱动器列表，包含盘符、类型、总空间、可用空间等。

#### `list_processes`
获取运行中的进程列表。

**返回**：进程列表，包含PID、名称、路径、内存使用等。

#### `list_windows`
获取桌面窗口列表。

**返回**：窗口列表，包含句柄、标题、类名、位置、大小等。

### 剪贴板工具

#### `read_clipboard`
读取剪贴板内容。

**返回**：支持文本、图片、文件三种类型。

#### `write_clipboard`
写入内容到剪贴板。

**参数**：
- `content` (string, 必需): 要写入的内容

### 截图工具

#### `take_screenshot`
截取整个屏幕。

**参数**：
- `format` (string, 可选): 图片格式（png/jpg，默认png）
- `quality` (integer, 可选): JPEG质量（1-100，默认90）

#### `take_region_screenshot`
截取屏幕区域。

**参数**：
- `x` (integer, 必需): 起始X坐标
- `y` (integer, 必需): 起始Y坐标
- `width` (integer, 必需): 宽度
- `height` (integer, 必需): 高度
- `format` (string, 可选): 图片格式

#### `take_window_screenshot`
截取指定窗口。

**参数**：
- `window_title` (string, 可选): 窗口标题（部分匹配）
- `process_name` (string, 可选): 进程名

### 电源管理工具

#### `shutdown`
关闭计算机。

#### `restart`
重启计算机。

#### `sleep`
使计算机进入休眠状态。

### 执行命令工具

#### `execute_command`
执行系统命令。

**参数**：
- `command` (string, 必需): 要执行的命令
- `timeout` (integer, 可选): 超时时间（秒，默认30）

#### `execute_powershell`
执行PowerShell命令。

**参数**：
- `script` (string, 必需): PowerShell脚本
- `timeout` (integer, 可选): 超时时间（秒，默认30）

### 浏览器工具

#### `list_browser_tabs`
列出浏览器标签页。

**参数**：
- `browser` (string, 可选): 浏览器类型（chrome/edge/firefox，自动检测）

#### `close_browser_tab`
关闭浏览器标签页。

**参数**：
- `tab_id` (string, 必需): 标签页ID
- `browser` (string, 可选): 浏览器类型

## 错误处理

所有工具调用都遵循标准的 MCP 错误响应格式：

```json
{
  "content": [
    {
      "type": "text",
      "text": "错误信息"
    }
  ],
  "isError": true
}
```

常见错误：
- 文件不存在
- 权限不足
- 路径无效
- 命令执行失败
- 超时

## 审计日志

开源版本提供完整的操作审计，所有操作都会记录到日志文件中：

- 日志位置：`%LOCALAPPDATA%\WinBridgeAgent\logs\audit-YYYY-MM-DD.log`
- 记录内容：时间戳、操作类型、参数、结果、客户端信息

## 安全说明

开源版本移除了商业版的安全限制，但请注意：

1. **网络安全**：如需局域网访问，请在设置中将监听地址改为 `0.0.0.0`
2. **防火墙**：确保 Windows 防火墙允许端口 35182 的访问
3. **权限**：程序以当前用户权限运行，无法执行需要管理员权限的操作

## 更多信息

- 项目主页：https://github.com/codyard/WinBridgeAgent
- MCP Registry：https://registry.modelcontextprotocol.io/v0.1/servers?search=io.github.codyard/winbridgeagent
- 完整文档：README.md / README_zh.md
