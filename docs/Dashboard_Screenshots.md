# Dashboard 界面说明

## 窗口布局

```
┌─────────────────────────────────────────────────────────────────┐
│ ClawDesk MCP Server - Dashboard                    [_] [□] [X] │  ← 标题栏（置顶窗口）
├─────────────────────────────────────────────────────────────────┤
│ Real-time monitoring - Logs are displayed below                 │  ← 说明文字
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [18:30:45.123] [REQ] HTTP - GET /status HTTP/1.1              │  ← 请求日志
│  [18:30:45.125] [PRO] status - Retrieving server status...     │  ← 处理日志
│  [18:30:45.128] [OK ] status - Status returned: port=35182     │  ← 成功日志
│        license=free                                             │  ← 详细信息（缩进）
│                                                                 │
│  [18:31:02.456] [REQ] HTTP - POST /exit HTTP/1.1               │
│  [18:31:02.458] [PRO] exit - Shutting down server...           │
│  [18:31:02.460] [OK ] exit - Shutdown command sent             │
│                                                                 │
│  [18:32:15.789] [REQ] HTTP - GET /unknown HTTP/1.1             │
│  [18:32:15.790] [ERR] HTTP - Endpoint not found                │  ← 错误日志
│        GET /unknown HTTP/1.1                                    │
│                                                                 │
│  [18:33:00.001] [REQ] system - Server starting...              │
│  [18:33:00.050] [PRO] firewall - Checking firewall rules...    │
│  [18:33:00.100] [OK ] firewall - Firewall rule exists          │
│  [18:33:00.150] [PRO] http_server - Starting HTTP server...    │
│  [18:33:00.200] [OK ] http_server - Started successfully       │
│  [18:33:00.250] [OK ] system - Server started on port 35182    │
│                                                                 │
│  ▼ (自动滚动到底部)                                             │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ [Clear Logs]  [Copy All]                                        │  ← 操作按钮
└─────────────────────────────────────────────────────────────────┘
     ↑              ↑
  清空日志      复制所有日志
```

## 日志格式详解

### 基本格式

```
[时间戳] [类型] 工具名 - 消息
      详细信息（可选，缩进显示）
```

### 时间戳格式

```
[HH:MM:SS.mmm]
 ↑  ↑  ↑  ↑
 时 分 秒 毫秒
```

示例：`[18:30:45.123]` = 18时30分45秒123毫秒

### 类型标记

| 标记    | 含义                 | 颜色（未来） | 示例         |
| ------- | -------------------- | ------------ | ------------ |
| `[REQ]` | Request（请求）      | 蓝色         | 接收到新请求 |
| `[PRO]` | Processing（处理中） | 黄色         | 正在处理操作 |
| `[OK ]` | Success（成功）      | 绿色         | 操作成功完成 |
| `[ERR]` | Error（错误）        | 红色         | 发生错误     |

### 工具名

常见的工具名包括：

- `system` - 系统事件（启动、停止）
- `HTTP` - HTTP 请求
- `firewall` - 防火墙配置
- `http_server` - HTTP 服务器
- `health` - 健康检查
- `status` - 状态查询
- `exit` - 退出命令

未来还会包括：

- `screenshot_full` - 截图
- `find_files` - 文件搜索
- `read_text_file` - 文件读取
- `clipboard_read` - 剪贴板读取
- `clipboard_write` - 剪贴板写入
- `list_windows` - 窗口列表
- `launch_app` - 应用启动
- `close_app` - 应用关闭
- `run_command_restricted` - 命令执行

## 典型日志序列

### 1. 服务器启动序列

```
[18:00:00.001] [REQ] system - Server starting...
[18:00:00.050] [PRO] firewall - Checking firewall rules...
[18:00:00.100] [OK ] firewall - Firewall rule already exists
[18:00:00.150] [PRO] http_server - Starting HTTP server...
[18:00:00.200] [OK ] http_server - HTTP server started successfully
[18:00:00.250] [OK ] system - Server started successfully on port 35182
```

### 2. HTTP 请求处理序列

```
[18:30:45.123] [REQ] HTTP - GET /status HTTP/1.1
[18:30:45.125] [PRO] status - Retrieving server status...
[18:30:45.128] [OK ] status - Status returned: port=35182, license=free
```

### 3. 错误处理序列

```
[18:32:15.789] [REQ] HTTP - GET /unknown HTTP/1.1
[18:32:15.790] [ERR] HTTP - Endpoint not found: GET /unknown HTTP/1.1
```

### 4. 服务器关闭序列

```
[18:35:00.001] [REQ] HTTP - POST /exit HTTP/1.1
[18:35:00.002] [PRO] exit - Shutting down server...
[18:35:00.003] [OK ] exit - Shutdown command sent
[18:35:00.100] [PRO] system - Server shutting down...
```

### 5. 防火墙配置序列

```
[18:00:00.050] [PRO] firewall - Checking firewall rules...
[18:00:00.100] [PRO] firewall - Firewall rule not found, requesting user permission...
[18:00:05.000] [PRO] firewall - Adding firewall rule...
[18:00:08.000] [OK ] firewall - Firewall rule added successfully
```

或者用户取消：

```
[18:00:00.050] [PRO] firewall - Checking firewall rules...
[18:00:00.100] [PRO] firewall - Firewall rule not found, requesting user permission...
[18:00:03.000] [PRO] firewall - User cancelled firewall configuration
```

## 窗口特性

### 1. 置顶显示

- Dashboard 窗口使用 `WS_EX_TOPMOST` 样式
- 始终显示在其他窗口上方
- 即使切换到其他应用，Dashboard 仍然可见

### 2. 自动滚动

- 新日志添加时自动滚动到底部
- 使用 `EM_SETSEL` 和 `EM_SCROLLCARET` 实现
- 用户可以手动滚动查看历史日志

### 3. 等宽字体

- 使用 Consolas 字体（16px）
- 确保日志对齐整齐
- 便于阅读时间戳和类型标记

### 4. 日志限制

- 最多保留 1000 条日志
- 超过限制时自动删除最旧的日志
- 使用 `std::deque` 实现高效的头部删除

## 操作按钮

### Clear Logs 按钮

- 位置：左下角
- 功能：清空所有日志
- 快捷键：无（未来可添加）
- 效果：
    - 日志显示区清空
    - 状态标签显示 "Logs cleared"
    - 日志计数重置为 0

### Copy All 按钮

- 位置：Clear Logs 右侧
- 功能：复制所有日志到剪贴板
- 快捷键：无（未来可添加）
- 效果：
    - 所有日志复制到系统剪贴板
    - 弹出提示 "Logs copied to clipboard"
    - 可以粘贴到任何文本编辑器

## 状态标签

位于窗口顶部，显示当前状态：

- 默认：`Real-time monitoring - Logs are displayed below`
- 清空后：`Logs cleared`
- 日志计数：`Total logs: 45 (max 1000)`

## 未来改进

### 1. 日志高亮

不同类型的日志使用不同颜色：

- `[REQ]` - 蓝色
- `[PRO]` - 黄色
- `[OK ]` - 绿色
- `[ERR]` - 红色

### 2. 日志过滤

添加过滤选项：

```
[Filter: All ▼] [Search: ________]
```

- 按类型过滤（All / Request / Processing / Success / Error）
- 按工具名过滤
- 搜索功能

### 3. 导出功能

添加导出按钮：

```
[Clear Logs] [Copy All] [Export...]
```

- 导出为 TXT 文件
- 导出为 JSON 文件
- 导出为 CSV 文件

### 4. 统计信息

在状态栏显示统计：

```
Total: 100 | Requests: 25 | Success: 23 | Errors: 2 | Processing: 0
```

### 5. 时间范围选择

添加时间范围过滤：

```
[Last 5 min ▼] [Last hour] [Today] [All]
```

## 技术细节

### 窗口样式

```cpp
WS_EX_TOPMOST       // 置顶
WS_EX_TOOLWINDOW    // 工具窗口（不在任务栏显示）
WS_OVERLAPPEDWINDOW // 标准窗口（可调整大小）
```

### 文本框样式

```cpp
WS_VSCROLL          // 垂直滚动条
ES_MULTILINE        // 多行文本
ES_READONLY         // 只读
ES_AUTOVSCROLL      // 自动垂直滚动
```

### 字体设置

```cpp
CreateFont(
    16,                     // 高度
    0,                      // 宽度（自动）
    0, 0,                   // 角度
    FW_NORMAL,              // 粗细
    FALSE, FALSE, FALSE,    // 斜体、下划线、删除线
    DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    DEFAULT_QUALITY,
    FIXED_PITCH | FF_MODERN, // 等宽字体
    "Consolas"              // 字体名称
);
```

### 线程安全

```cpp
std::mutex logMutex_;           // 保护日志队列
std::deque<DashboardLogEntry>   // 日志存储
```

- 使用 `std::lock_guard` 自动加锁/解锁
- 支持多线程并发写入日志
- UI 更新在主线程中执行

## 相关文档

- [DASHBOARD_GUIDE.md](DASHBOARD_GUIDE.md) - 快速使用指南
- [Dashboard.md](Dashboard.md) - 详细技术文档
- [README.md](../README.md) - 主要文档
