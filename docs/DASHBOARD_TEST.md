# Dashboard 测试说明

## 问题排查

如果 Dashboard 窗口没有显示，请按以下步骤检查：

### 1. 确认程序正在运行

- 检查系统托盘（任务栏右下角）是否有 ClawDesk 图标
- 如果没有，双击 `ClawDeskMCP-x64.exe` 启动程序

### 2. 打开 Dashboard

1. **右键点击**托盘图标（不是左键）
2. 在弹出菜单中找到 **"Dashboard"** 选项
3. **左键点击** "Dashboard" 菜单项

### 3. 检查窗口是否被遮挡

- Dashboard 是置顶窗口，应该在所有窗口最前面
- 检查屏幕四周，窗口可能在屏幕边缘
- 尝试按 `Alt+Tab` 切换窗口，看是否能找到 Dashboard

### 4. 查看错误信息

如果 Dashboard 创建失败，程序会弹出错误对话框：

- "Failed to register window class" - 窗口类注册失败
- "Failed to create Dashboard window" - 窗口创建失败

记录错误代码（Error: xxxxx）并报告。

### 5. 重启程序

1. 右键托盘图标，选择 "Exit"
2. 等待程序完全退出
3. 重新启动 `ClawDeskMCP-x64.exe`
4. 再次尝试打开 Dashboard

## 测试步骤

### 步骤 1：启动程序

```bash
# 在 Windows 上双击运行
ClawDeskMCP-x64.exe
```

或者从命令行运行（可以看到错误信息）：

```cmd
cd C:\path\to\ClawDeskMCP
ClawDeskMCP-x64.exe
```

### 步骤 2：打开 Dashboard

1. 找到托盘图标（任务栏右下角）
2. 右键点击图标
3. 点击 "Dashboard"

**预期结果**：

- 弹出一个 800x600 的窗口
- 标题为 "ClawDesk MCP Server - Dashboard"
- 窗口置顶显示
- 显示之前记录的日志（如果有）

### 步骤 3：测试日志记录

打开 Dashboard 后，在另一个终端或浏览器中测试：

```bash
# 测试 1：健康检查
curl http://192.168.31.3:35182/health

# 测试 2：状态查询
curl http://192.168.31.3:35182/status

# 测试 3：根路径
curl http://192.168.31.3:35182/
```

**预期结果**：

Dashboard 中应该显示类似以下的日志：

```
[18:30:45.123] [REQ] HTTP - GET /health HTTP/1.1
[18:30:45.125] [OK ] health - Health check OK

[18:30:50.456] [REQ] HTTP - GET /status HTTP/1.1
[18:30:50.458] [PRO] status - Retrieving server status...
[18:30:50.460] [OK ] status - Status returned: port=35182, license=free

[18:30:55.789] [REQ] HTTP - GET / HTTP/1.1
[18:30:55.790] [OK ] root - Welcome message sent
```

### 步骤 4：测试操作按钮

#### 测试 Clear Logs

1. 点击 "Clear Logs" 按钮
2. **预期结果**：所有日志被清空，显示区变为空白

#### 测试 Copy All

1. 发送几个请求产生日志
2. 点击 "Copy All" 按钮
3. **预期结果**：弹出提示 "Logs copied to clipboard"
4. 打开记事本，按 `Ctrl+V` 粘贴
5. **预期结果**：看到所有日志内容

### 步骤 5：测试显示/隐藏

1. 再次右键点击托盘图标
2. 点击 "Dashboard"
3. **预期结果**：Dashboard 窗口隐藏
4. 再次点击 "Dashboard"
5. **预期结果**：Dashboard 窗口重新显示，之前的日志仍然存在

## 调试信息

### 检查日志文件

即使 Dashboard 不显示，日志仍会记录到文件：

```bash
# 查看审计日志
cat logs/audit-2026-02-03.log
```

### 检查配置文件

```bash
# 查看配置
cat config.json
```

确认以下配置：

- `server_port`: 35182
- `listen_address`: "0.0.0.0" 或 "127.0.0.1"

### 检查进程

```cmd
# Windows 命令提示符
tasklist | findstr ClawDesk

# PowerShell
Get-Process | Where-Object {$_.ProcessName -like "*ClawDesk*"}
```

应该看到 `ClawDeskMCP.exe` 进程。

### 检查端口

```cmd
# 检查端口是否被占用
netstat -ano | findstr :35182
```

应该看到端口 35182 处于 LISTENING 状态。

## 常见问题

### Q: 点击 Dashboard 菜单没有反应？

**A**: 可能的原因：

1. 窗口创建失败（应该有错误对话框）
2. 窗口被创建但不可见（检查任务管理器）
3. 程序崩溃（检查事件查看器）

**解决方法**：

1. 重启程序
2. 检查 Windows 事件查看器中的应用程序日志
3. 尝试以管理员身份运行

### Q: Dashboard 窗口一闪而过？

**A**: 这可能是窗口创建后立即被销毁。

**解决方法**：

1. 检查是否有错误对话框
2. 从命令行运行程序，查看错误输出
3. 检查是否有杀毒软件阻止

### Q: Dashboard 显示但是没有日志？

**A**: 可能的原因：

1. 日志记录功能未正常工作
2. 还没有产生任何日志

**解决方法**：

1. 发送一些 HTTP 请求测试
2. 检查 `logs/audit-*.log` 文件是否有内容
3. 点击 "Clear Logs" 然后重新测试

### Q: Dashboard 窗口太小/太大？

**A**: 当前窗口大小固定为 800x600。

**未来改进**：

- 支持调整窗口大小
- 记住窗口位置和大小
- 支持最大化/最小化

## 报告问题

如果 Dashboard 仍然无法显示，请提供以下信息：

1. **Windows 版本**：Windows 10/11，32位/64位
2. **错误信息**：截图或复制错误对话框内容
3. **日志文件**：`logs/audit-*.log` 的内容
4. **配置文件**：`config.json` 的内容
5. **进程信息**：`tasklist` 的输出
6. **端口信息**：`netstat -ano | findstr :35182` 的输出

## 成功标志

Dashboard 正常工作时，你应该看到：

✅ 托盘图标正常显示
✅ 右键菜单中有 "Dashboard" 选项
✅ 点击后弹出 Dashboard 窗口
✅ 窗口标题为 "ClawDesk MCP Server - Dashboard"
✅ 窗口置顶显示（在所有窗口最前面）
✅ 发送 HTTP 请求后，Dashboard 中实时显示日志
✅ 日志格式正确：`[时间] [类型] 工具名 - 消息`
✅ "Clear Logs" 按钮可以清空日志
✅ "Copy All" 按钮可以复制日志到剪贴板
✅ 再次点击 "Dashboard" 可以隐藏/显示窗口

## 下一步

如果 Dashboard 正常工作，你可以：

1. 运行测试脚本：`../scripts/test_dashboard.sh`
2. 阅读详细文档：`docs/Dashboard.md`
3. 查看使用指南：`DASHBOARD_GUIDE.md`
4. 继续开发其他功能（MCP 协议、工具实现等）
