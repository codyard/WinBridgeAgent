# Dashboard 快速使用指南

## 什么是 Dashboard？

Dashboard 是 ClawDesk MCP Server 的实时监控窗口，让你可以直观地看到：

- 服务器接收到的每一个请求
- 正在处理的操作
- 操作的成功或失败结果
- 详细的错误信息

## 如何打开 Dashboard？

### 方法 1：通过托盘菜单

1. 找到系统托盘（任务栏右下角）的 ClawDesk 图标
2. **右键点击**图标
3. 在弹出菜单中选择 **"Dashboard"**
4. Dashboard 窗口会立即弹出

### 方法 2：再次点击隐藏

- 如果 Dashboard 已经打开，再次点击 "Dashboard" 菜单项会隐藏窗口
- 这样可以快速切换显示/隐藏状态

## Dashboard 界面说明

```
┌─────────────────────────────────────────────────────────┐
│ ClawDesk MCP Server - Dashboard                    [_][□][X]│
├─────────────────────────────────────────────────────────┤
│ Total logs: 45 (max 1000)                               │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ [18:30:45.123] [REQ] HTTP - GET /status HTTP/1.1       │
│ [18:30:45.125] [PRO] status - Retrieving status...     │
│ [18:30:45.128] [OK ] status - Status returned          │
│       port=35182, license=free                          │
│                                                         │
│ [18:31:02.456] [REQ] HTTP - POST /exit HTTP/1.1        │
│ [18:31:02.458] [PRO] exit - Shutting down server...    │
│ [18:31:02.460] [OK ] exit - Shutdown command sent      │
│                                                         │
│ [18:32:15.789] [ERR] HTTP - Endpoint not found         │
│       GET /unknown HTTP/1.1                             │
│                                                         │
│                                                         │
├─────────────────────────────────────────────────────────┤
│ [Clear Logs]  [Copy All]                                │
└─────────────────────────────────────────────────────────┘
```

### 界面元素

1. **状态栏**（顶部）
    - 显示当前日志总数
    - 显示最大日志限制（1000 条）

2. **日志显示区**（中间）
    - 使用等宽字体（Consolas）
    - 自动滚动到最新日志
    - 支持鼠标滚轮查看历史日志

3. **操作按钮**（底部）
    - **Clear Logs**: 清空所有日志
    - **Copy All**: 复制所有日志到剪贴板

## 日志类型说明

Dashboard 使用不同的标记来区分日志类型：

| 标记    | 类型   | 说明           | 示例                           |
| ------- | ------ | -------------- | ------------------------------ |
| `[REQ]` | 请求   | 接收到新的请求 | `[REQ] HTTP - GET /status`     |
| `[PRO]` | 处理中 | 正在处理操作   | `[PRO] status - Retrieving...` |
| `[OK ]` | 成功   | 操作成功完成   | `[OK ] status - Completed`     |
| `[ERR]` | 错误   | 发生错误       | `[ERR] HTTP - Not found`       |

## 实际使用场景

### 场景 1：监控服务器启动

打开 Dashboard，然后启动服务器，你会看到：

```
[18:00:00.001] [REQ] system - Server starting...
[18:00:00.050] [PRO] firewall - Checking firewall rules...
[18:00:00.100] [OK ] firewall - Firewall rule already exists
[18:00:00.150] [PRO] http_server - Starting HTTP server...
[18:00:00.200] [OK ] http_server - HTTP server started successfully
[18:00:00.250] [OK ] system - Server started successfully on port 35182
```

### 场景 2：监控 HTTP 请求

当有客户端访问服务器时：

```
[18:30:45.123] [REQ] HTTP - GET /status HTTP/1.1
[18:30:45.125] [PRO] status - Retrieving server status...
[18:30:45.128] [OK ] status - Status returned: port=35182, license=free
```

### 场景 3：调试错误

当请求失败时，Dashboard 会显示详细错误：

```
[18:32:15.789] [REQ] HTTP - GET /unknown HTTP/1.1
[18:32:15.790] [ERR] HTTP - Endpoint not found: GET /unknown HTTP/1.1
```

### 场景 4：监控服务器关闭

当发送退出命令时：

```
[18:35:00.001] [REQ] HTTP - POST /exit HTTP/1.1
[18:35:00.002] [PRO] exit - Shutting down server...
[18:35:00.003] [OK ] exit - Shutdown command sent
[18:35:00.100] [PRO] system - Server shutting down...
```

## 常用操作

### 清空日志

如果日志太多，影响查看：

1. 点击 **"Clear Logs"** 按钮
2. 所有日志会被清空
3. 状态栏显示 "Logs cleared"

### 复制日志

需要保存或分享日志时：

1. 点击 **"Copy All"** 按钮
2. 所有日志会被复制到剪贴板
3. 弹出提示 "Logs copied to clipboard"
4. 打开记事本或其他文本编辑器
5. 按 `Ctrl+V` 粘贴

### 查看历史日志

1. 使用鼠标滚轮向上滚动
2. 或拖动右侧滚动条
3. 查看之前的日志记录

## 测试 Dashboard

### 使用 curl 测试

打开 Dashboard，然后在命令行执行：

```bash
# 测试健康检查
curl http://localhost:35182/health

# 测试状态查询
curl http://localhost:35182/status

# 测试根路径
curl http://localhost:35182/
```

你会在 Dashboard 中看到每个请求的完整处理过程。

### 使用浏览器测试

1. 打开 Dashboard
2. 在浏览器中访问：`http://localhost:35182/status`
3. 观察 Dashboard 中的日志变化

### 使用 PowerShell 测试

```powershell
# 测试状态
Invoke-WebRequest -Uri "http://localhost:35182/status"

# 测试健康检查
Invoke-WebRequest -Uri "http://localhost:35182/health"
```

## 高级技巧

### 1. 保持 Dashboard 可见

Dashboard 窗口是置顶的（Top-most），会始终显示在其他窗口上方。这样即使你在使用其他程序，也能随时看到服务器的运行状态。

### 2. 日志过滤（未来功能）

目前 Dashboard 显示所有日志。未来版本将支持：

- 按类型过滤（只看错误、只看请求等）
- 按工具名过滤
- 搜索功能

### 3. 导出日志

虽然 Dashboard 只保留最近 1000 条日志，但你可以：

1. 定期点击 "Copy All" 保存日志
2. 或查看 `logs/audit-*.log` 文件获取完整历史

### 4. 性能考虑

- Dashboard 只在可见时更新 UI
- 隐藏 Dashboard 不会影响日志记录
- 日志记录对性能影响极小

## 故障排除

### Q: Dashboard 窗口不显示？

**A**: 检查以下几点：

1. 确认已点击托盘菜单中的 "Dashboard"
2. 检查是否被其他窗口遮挡（Dashboard 应该是置顶的）
3. 尝试关闭再重新打开

### Q: 日志显示不完整？

**A**: Dashboard 最多保留 1000 条日志。如果需要完整日志：

1. 定期使用 "Copy All" 保存
2. 查看 `logs/audit-*.log` 文件

### Q: 如何关闭 Dashboard？

**A**: 三种方法：

1. 点击窗口右上角的 X 按钮
2. 再次点击托盘菜单中的 "Dashboard"
3. 关闭不会停止服务器，只是隐藏窗口

### Q: Dashboard 影响性能吗？

**A**: 影响非常小：

- 只有在可见时才更新 UI
- 日志记录使用高效的数据结构
- 限制最大日志数量避免内存增长

## 相关文档

- [README.md](README.md) - 主要文档
- [docs/Dashboard.md](docs/Dashboard.md) - 详细技术文档
- [FIREWALL.md](FIREWALL.md) - 防火墙配置指南

## 反馈和建议

如果你对 Dashboard 有任何建议或发现问题，欢迎反馈！
