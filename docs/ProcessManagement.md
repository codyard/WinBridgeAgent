# 进程管理 - Process Management

## 概述

ClawDesk MCP Server v0.3.0 提供了强大的进程管理功能，允许 AI 助手终止进程和调整进程优先级。这些功能经过精心设计，在提供灵活性的同时确保系统安全。

## 功能列表

### 1. 进程终止 (kill_process)

终止指定 PID 的进程，支持优雅关闭和强制终止两种模式。

### 2. 进程优先级调整 (set_process_priority)

调整进程的 CPU 调度优先级，优化系统性能。

---

## 进程终止

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "kill_process",
    "arguments": {
        "pid": 1234,
        "force": false
    }
}
```

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/process/kill \
  -H "Content-Type: application/json" \
  -d '{"pid": 1234, "force": false}'
```

### 参数说明

- **pid** (number, 必需): 进程 ID
    - 可以通过 `list_processes` 工具获取
    - 必须是有效的正整数
- **force** (boolean, 可选): 是否强制终止，默认 false
    - `false`: 优雅关闭（先发送 WM_CLOSE 消息）
    - `true`: 强制终止（直接调用 TerminateProcess）

### 终止模式

#### 优雅关闭 (force=false)

1. 枚举进程的所有窗口
2. 向每个窗口发送 WM_CLOSE 消息
3. 等待进程自行退出（最多 5 秒）
4. 如果进程未退出，则使用 TerminateProcess 强制终止

**优点**:

- 允许应用程序保存数据
- 执行清理操作
- 更安全，不会导致数据丢失

**适用场景**:

- 关闭有界面的应用程序
- 需要保存数据的进程
- 正常的进程管理

#### 强制终止 (force=true)

直接调用 TerminateProcess API 强制终止进程。

**优点**:

- 立即生效
- 适用于无响应的进程

**缺点**:

- 可能导致数据丢失
- 不执行清理操作
- 可能留下临时文件

**适用场景**:

- 进程无响应（卡死）
- 恶意进程
- 紧急情况

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "pid": 1234,
    "process_name": "notepad.exe",
    "forced": false
}
```

**字段说明**:

- `success`: 是否成功终止
- `pid`: 进程 ID
- `process_name`: 进程名称
- `forced`: 是否使用了强制终止

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

### 受保护进程

以下系统进程受到硬编码保护，**无法被终止**：

| 进程名称     | 说明                 |
| ------------ | -------------------- |
| system       | Windows 内核进程     |
| csrss.exe    | 客户端/服务器运行时  |
| winlogon.exe | Windows 登录进程     |
| services.exe | 服务控制管理器       |
| lsass.exe    | 本地安全授权子系统   |
| smss.exe     | 会话管理器子系统     |
| wininit.exe  | Windows 启动应用程序 |

**设计原因**:

- 这些进程是 Windows 系统的核心组件
- 终止它们会导致系统崩溃或蓝屏
- 硬编码在 `ProcessService::PROTECTED_PROCESSES` 中
- 不可通过配置文件修改

### 安全机制

1. **受保护进程检查**: 在终止前检查进程名称
2. **用户确认**: 高风险操作，需要用户确认（通过 PolicyGuard）
3. **审计日志**: 所有终止操作都会记录到审计日志
4. **权限检查**: 需要足够的权限才能终止进程

### 审计日志示例

```json
{
    "time": "2026-02-04T10:30:00.000Z",
    "tool": "kill_process",
    "risk": "high",
    "result": "ok",
    "duration_ms": 15,
    "high_risk": true,
    "details": {
        "pid": 1234,
        "process_name": "notepad.exe",
        "forced": false
    }
}
```

---

## 进程优先级调整

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "set_process_priority",
    "arguments": {
        "pid": 1234,
        "priority": "high"
    }
}
```

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/process/priority \
  -H "Content-Type: application/json" \
  -d '{"pid": 1234, "priority": "high"}'
```

### 参数说明

- **pid** (number, 必需): 进程 ID
- **priority** (string, 必需): 优先级级别

### 优先级级别

| 级别         | 说明                         | 使用场景                 |
| ------------ | ---------------------------- | ------------------------ |
| idle         | 空闲优先级                   | 后台任务，不影响其他进程 |
| below_normal | 低于正常优先级               | 低优先级后台任务         |
| normal       | 正常优先级（默认）           | 大多数应用程序           |
| above_normal | 高于正常优先级               | 需要更多 CPU 时间的应用  |
| high         | 高优先级                     | 重要的前台应用程序       |
| realtime     | 实时优先级（需要管理员权限） | 实时系统、驱动程序       |

### 优先级映射

内部使用 Windows API 的优先级类：

| ClawDesk 级别 | Windows API 常量            |
| ------------- | --------------------------- |
| idle          | IDLE_PRIORITY_CLASS         |
| below_normal  | BELOW_NORMAL_PRIORITY_CLASS |
| normal        | NORMAL_PRIORITY_CLASS       |
| above_normal  | ABOVE_NORMAL_PRIORITY_CLASS |
| high          | HIGH_PRIORITY_CLASS         |
| realtime      | REALTIME_PRIORITY_CLASS     |

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "pid": 1234,
    "old_priority": "normal",
    "new_priority": "high"
}
```

**字段说明**:

- `success`: 是否成功调整
- `pid`: 进程 ID
- `old_priority`: 调整前的优先级
- `new_priority`: 调整后的优先级

**错误响应**:

```json
{
    "error": "Insufficient privileges for realtime priority"
}
```

```json
{
    "error": "Process not found or access denied"
}
```

### 实时优先级注意事项

**realtime** 优先级具有特殊性：

1. **需要管理员权限**: 必须以管理员身份运行 ClawDesk Server
2. **系统风险**: 可能导致系统无响应
3. **谨慎使用**: 仅用于真正需要实时响应的应用程序

**警告**: 不当使用 realtime 优先级可能导致：

- 系统卡顿
- 其他进程无法获得 CPU 时间
- 鼠标和键盘无响应

### 性能优化建议

#### 降低优先级

适用于：

- 后台下载任务
- 批处理作业
- 数据备份

```json
{
    "name": "set_process_priority",
    "arguments": {
        "pid": 5678,
        "priority": "below_normal"
    }
}
```

#### 提高优先级

适用于：

- 视频编辑软件
- 游戏
- 实时通信应用

```json
{
    "name": "set_process_priority",
    "arguments": {
        "pid": 9012,
        "priority": "above_normal"
    }
}
```

### 审计日志示例

```json
{
    "time": "2026-02-04T10:35:00.000Z",
    "tool": "set_process_priority",
    "risk": "medium",
    "result": "ok",
    "duration_ms": 8,
    "details": {
        "pid": 1234,
        "old_priority": "normal",
        "new_priority": "high"
    }
}
```

---

## 使用场景

### 场景 1: 关闭无响应的应用程序

**问题**: 应用程序卡死，无法正常关闭

**解决方案**:

1. 列出进程找到 PID
2. 先尝试优雅关闭
3. 如果失败，使用强制终止

```bash
# 步骤 1: 列出进程
curl http://localhost:35182/processes

# 步骤 2: 优雅关闭
curl -X POST http://localhost:35182/process/kill \
  -H "Content-Type: application/json" \
  -d '{"pid": 1234, "force": false}'

# 步骤 3: 如果失败，强制终止
curl -X POST http://localhost:35182/process/kill \
  -H "Content-Type: application/json" \
  -d '{"pid": 1234, "force": true}'
```

### 场景 2: 优化视频渲染性能

**问题**: 视频渲染软件运行缓慢

**解决方案**: 提高渲染进程的优先级

```json
{
    "name": "set_process_priority",
    "arguments": {
        "pid": 5678,
        "priority": "high"
    }
}
```

### 场景 3: 后台任务不影响前台工作

**问题**: 后台备份任务占用太多 CPU

**解决方案**: 降低后台任务的优先级

```json
{
    "name": "set_process_priority",
    "arguments": {
        "pid": 9012,
        "priority": "idle"
    }
}
```

---

## 错误处理

### 常见错误

#### 1. 进程不存在

**错误**: `Process not found or access denied`

**原因**:

- PID 无效
- 进程已经退出
- 没有权限访问进程

**解决方案**:

- 使用 `list_processes` 工具获取最新的进程列表
- 确保 PID 正确
- 以管理员身份运行 ClawDesk Server

#### 2. 受保护进程

**错误**: `Process is protected and cannot be terminated`

**原因**: 尝试终止系统关键进程

**解决方案**: 这是设计行为，无法绕过

#### 3. 权限不足

**错误**: `Insufficient privileges for realtime priority`

**原因**: 设置 realtime 优先级需要管理员权限

**解决方案**: 以管理员身份运行 ClawDesk Server

---

## 最佳实践

### 1. 优先使用优雅关闭

除非进程无响应，否则始终使用 `force=false`。

### 2. 谨慎使用高优先级

不要随意提高进程优先级，可能影响系统整体性能。

### 3. 监控审计日志

定期检查审计日志，了解进程管理操作的历史。

### 4. 避免终止系统进程

即使不在受保护列表中，也要避免终止系统相关进程。

### 5. 测试优先级调整效果

调整优先级后，观察系统性能变化，必要时恢复原状。

---

## 技术实现

### ProcessService 类

```cpp
class ProcessService {
public:
    ProcessService(ConfigManager* configManager, PolicyGuard* policyGuard);

    // 终止进程
    KillProcessResult killProcess(DWORD pid, bool force);

    // 调整进程优先级
    SetPriorityResult setProcessPriority(DWORD pid, ProcessPriority priority);

private:
    // 检查进程是否受保护
    bool isProtectedProcess(const std::string& processName);

    // 尝试优雅关闭
    bool tryGracefulClose(DWORD pid);

    // 强制终止进程
    bool forceTerminate(DWORD pid);

    // 受保护的系统进程列表
    static const std::vector<std::string> PROTECTED_PROCESSES;
};
```

### 关键 API

- `OpenProcess()`: 打开进程句柄
- `TerminateProcess()`: 强制终止进程
- `EnumWindows()`: 枚举窗口
- `SendMessage(WM_CLOSE)`: 发送关闭消息
- `SetPriorityClass()`: 设置优先级
- `GetPriorityClass()`: 获取优先级

---

## 安全考虑

### 1. 受保护进程列表

硬编码在源代码中，不可配置，确保系统稳定性。

### 2. 用户确认

高风险操作（kill_process）需要用户通过 MessageBox 确认。

### 3. 审计日志

所有操作都会记录详细信息，包括：

- 时间戳
- 进程 ID 和名称
- 操作类型
- 是否强制终止
- 优先级变化

### 4. 权限检查

- 需要足够的权限才能操作进程
- realtime 优先级需要管理员权限

---

## 故障排除

### 问题 1: 无法终止进程

**症状**: 调用 kill_process 返回错误

**可能原因**:

1. 进程是受保护进程
2. 权限不足
3. 进程已经退出

**解决步骤**:

1. 检查进程名称是否在受保护列表中
2. 以管理员身份运行 ClawDesk Server
3. 使用 `list_processes` 确认进程仍在运行

### 问题 2: 优先级调整无效

**症状**: 调整优先级后性能没有改善

**可能原因**:

1. 进程本身不是性能瓶颈
2. 其他进程占用资源
3. 硬件限制

**解决步骤**:

1. 使用任务管理器确认优先级已更改
2. 检查 CPU 和内存使用情况
3. 考虑其他优化方法

---

## 参考资料

- [Windows Process API](https://docs.microsoft.com/en-us/windows/win32/procthread/processes-and-threads)
- [Process Priority Classes](https://docs.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities)
- [TerminateProcess Function](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess)

---

## 版本历史

- v0.3.0 (2026-02-04)
    - ✅ 初始实现
    - ✅ 进程终止（优雅关闭 + 强制终止）
    - ✅ 进程优先级调整
    - ✅ 受保护进程列表
    - ✅ 审计日志集成
