# Dashboard 功能文档

## 概述

Dashboard 是 ClawDesk MCP Server 的实时监控窗口，用于观察服务器接收的指令、处理过程和执行结果。

## 功能特性

### 1. 实时日志显示

Dashboard 会实时显示以下信息：

- **请求日志** `[REQ]`：接收到的 HTTP 请求
- **处理日志** `[PRO]`：正在处理的操作
- **成功日志** `[OK ]`：成功完成的操作
- **错误日志** `[ERR]`：发生的错误
- **高风险操作** `[!!!]`：高风险操作（v0.3.0 新增）

### 2. 高风险操作显示（v0.3.0）

Dashboard 现在会特别标记和统计高风险操作：

- **红色标记**：高风险操作在日志中用 `[!!!]` 标记
- **操作计数器**：右上角显示高风险操作执行次数
- **详细信息**：显示操作的详细参数（如 PID、文件路径等）

高风险操作包括：

- `kill_process` - 终止进程
- `delete_file` - 删除文件/目录
- `shutdown_system` - 系统关机/重启

### 3. 关机倒计时（v0.3.0）

当计划关机或重启时，Dashboard 会显示：

- **倒计时横幅**：顶部显示红色警告横幅
- **剩余时间**：实时更新剩余秒数
- **取消按钮**：点击 "Cancel" 按钮可请求取消关机

### 4. 窗口特性

- **置顶显示**：窗口始终保持在最前面（Top-most）
- **自动滚动**：新日志自动滚动到底部
- **等宽字体**：使用 Consolas 字体，便于阅读
- **日志限制**：最多保留 1000 条日志，超过后自动删除旧日志

### 5. 操作按钮

- **Clear Logs**：清空所有日志
- **Copy All**：复制所有日志到剪贴板
- **Cancel**：取消计划的关机/重启（仅在倒计时显示时可见）

## 使用方法

### 打开 Dashboard

1. 右键点击系统托盘图标
2. 选择 "Dashboard" 菜单项
3. Dashboard 窗口会弹出并置顶显示

### 关闭 Dashboard

- 点击窗口的关闭按钮（X）
- 或再次点击托盘菜单中的 "Dashboard"

**注意**：关闭 Dashboard 不会停止服务器，只是隐藏窗口。

## 日志格式

每条日志的格式如下：

```
[时间戳] [类型] 工具名 - 消息
      详细信息（如果有）
```

### 示例

```
[18:30:45.123] [REQ] HTTP - GET /status HTTP/1.1
[18:30:45.125] [PRO] status - Retrieving server status...
[18:30:45.128] [OK ] status - Status returned: port=35182, license=free

[18:31:02.456] [REQ] HTTP - POST /exit HTTP/1.1
[18:31:02.458] [PRO] exit - Shutting down server...
[18:31:02.460] [OK ] exit - Shutdown command sent

[18:32:15.789] [ERR] HTTP - Endpoint not found: GET /unknown HTTP/1.1

[18:35:20.123] [!!!] kill_process - HIGH RISK OPERATION
      {"pid": 1234, "process_name": "notepad.exe", "forced": false}

[18:36:45.456] [!!!] shutdown_system - HIGH RISK OPERATION
      {"action": "shutdown", "delay": 60, "scheduled_time": "2026-02-03T18:37:45Z"}
```

## 监控的事件

Dashboard 会记录以下事件：

### 系统事件

- `system` - 服务器启动/停止
- `firewall` - 防火墙规则检查和配置
- `http_server` - HTTP 服务器启动

### HTTP 请求

- `HTTP` - 接收到的 HTTP 请求
- `health` - 健康检查请求
- `status` - 状态查询请求
- `exit` - 退出命令
- `root` - 根路径访问

### 未来扩展

当实现完整的 MCP 协议后，Dashboard 还会显示：

- `screenshot_full` - 截图请求
- `find_files` - 文件搜索请求
- `read_text_file` - 文件读取请求
- `clipboard_read` - 剪贴板读取
- `clipboard_write` - 剪贴板写入
- `list_windows` - 窗口列表查询
- `launch_app` - 应用启动
- `close_app` - 应用关闭
- `run_command_restricted` - 命令执行

### v0.3.0 新增工具

- `kill_process` - 终止进程（高风险）
- `set_process_priority` - 调整进程优先级
- `delete_file` - 删除文件/目录（高风险）
- `copy_file` - 复制文件/目录
- `move_file` - 移动/重命名文件
- `create_directory` - 创建目录
- `shutdown_system` - 系统关机/重启/休眠/睡眠（高风险）
- `abort_shutdown` - 取消计划的关机/重启

## 技术实现

### 架构

```
┌─────────────────────────────────────────────────────────────┐
│                    DashboardWindow                          │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ [WARNING: System shutdown scheduled in 60 seconds]    │  │
│  │                                        [Cancel]        │  │
│  └───────────────────────────────────────────────────────┘  │
│  Status: Total logs: 45 (max 1000)  High-Risk Ops: 3       │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Log Display (Edit Control)                           │  │
│  │  - Multi-line                                         │  │
│  │  - Read-only                                          │  │
│  │  - Auto-scroll                                        │  │
│  │  - Consolas font                                      │  │
│  │  - High-risk operations marked with [!!!]            │  │
│  └───────────────────────────────────────────────────────┘  │
│  [Clear Logs]  [Copy All]                                   │
└─────────────────────────────────────────────────────────────┘
```

### 线程安全

- 使用 `std::mutex` 保护日志队列
- 支持多线程并发写入日志
- UI 更新在主线程中执行

### 性能优化

- 使用 `std::deque` 存储日志，高效的头部删除
- 限制最大日志数量，避免内存无限增长
- 批量更新 UI，减少刷新次数

## 调试技巧

### 1. 测试 HTTP 请求

使用 curl 测试并观察 Dashboard：

```bash
# 健康检查
curl http://192.168.31.3:35182/health

# 状态查询
curl http://192.168.31.3:35182/status

# 退出命令
curl http://192.168.31.3:35182/exit
```

### 2. 复制日志

点击 "Copy All" 按钮，将所有日志复制到剪贴板，然后粘贴到文本编辑器中进行分析。

### 3. 清空日志

如果日志太多，点击 "Clear Logs" 清空，重新开始记录。

## 常见问题

### Q: Dashboard 窗口不显示？

A: 检查是否被其他窗口遮挡。Dashboard 是置顶窗口，应该始终在最前面。

### Q: 日志显示不完整？

A: Dashboard 最多保留 1000 条日志。如果需要完整日志，请查看 `logs/audit-*.log` 文件。

### Q: 如何导出日志？

A: 点击 "Copy All" 按钮，然后粘贴到文本文件中保存。

### Q: Dashboard 影响性能吗？

A: 影响很小。只有在 Dashboard 可见时才会更新 UI。隐藏 Dashboard 后，日志仍会记录但不会更新显示。

### Q: 如何取消计划的关机？

A: 当 Dashboard 显示关机倒计时时，点击 "Cancel" 按钮。或者使用 `abort_shutdown` MCP 工具。

### Q: 高风险操作计数器会重置吗？

A: 计数器在服务器运行期间持续累加。重启服务器后会重置为 0。

## 相关文档

- [README.md](../README.md) - 主要文档
- [AuditLogger.md](AuditLogger.md) - 审计日志文档
- [ConfigManager.md](ConfigManager.md) - 配置管理文档

## 未来改进

- [ ] 支持日志过滤（按类型、工具名）
- [ ] 支持日志搜索
- [ ] 支持导出为文件
- [ ] 支持日志高亮显示（不同颜色）
- [ ] 支持统计图表（请求数量、成功率等）
- [ ] 支持实时性能监控（CPU、内存）
- [x] 高风险操作标记和计数（v0.3.0 已实现）
- [x] 关机倒计时显示（v0.3.0 已实现）
