# 电源管理 - Power Management

## 概述

ClawDesk MCP Server v0.3.0 提供了完整的电源管理功能，允许 AI 助手远程控制系统的电源状态。支持关机、重启、休眠和睡眠四种操作，并提供延迟执行和取消功能。

## 功能列表

### 1. 系统关机/重启/休眠/睡眠 (shutdown_system)

执行指定的电源操作，支持延迟执行和强制关闭应用程序。

### 2. 取消关机 (abort_shutdown)

取消计划的关机或重启操作。

---

## 系统关机/重启/休眠/睡眠

### 基本用法

**MCP 工具调用**:

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

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "shutdown", "delay": 60, "force": false, "message": "System will shutdown in 1 minute"}'
```

### 参数说明

- **action** (string, 必需): 电源操作类型
    - `shutdown`: 关机
    - `reboot`: 重启
    - `hibernate`: 休眠
    - `sleep`: 睡眠
- **delay** (number, 可选): 延迟时间（秒），默认 0
    - 0: 立即执行
    - > 0: 延迟指定秒数后执行
    - 最大值: 315360000（10 年）
- **force** (boolean, 可选): 是否强制关闭应用程序，默认 false
    - `false`: 允许应用程序保存数据并正常退出
    - `true`: 强制关闭所有应用程序
- **message** (string, 可选): 显示给用户的消息
    - 最大长度: 512 字符
    - 在延迟执行时显示在系统通知中

### 电源操作类型

#### 1. 关机 (shutdown)

完全关闭计算机电源。

**使用场景**:

- 远程关闭计算机
- 定时关机
- 节能管理

**示例**:

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown",
        "delay": 300,
        "message": "System will shutdown in 5 minutes. Please save your work."
    }
}
```

#### 2. 重启 (reboot)

重新启动计算机。

**使用场景**:

- 安装更新后重启
- 系统维护
- 远程重启

**示例**:

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "reboot",
        "delay": 60,
        "message": "System will reboot in 1 minute for updates."
    }
}
```

#### 3. 休眠 (hibernate)

将内存内容保存到硬盘并关闭电源。

**特点**:

- 保存当前工作状态
- 完全关闭电源
- 恢复时回到休眠前的状态
- 启动速度比冷启动快

**使用场景**:

- 长时间离开但需要保持工作状态
- 笔记本电脑电量不足
- 节能模式

**示例**:

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "hibernate"
    }
}
```

**注意**: 休眠功能需要系统支持，某些系统可能禁用了休眠功能。

#### 4. 睡眠 (sleep)

进入低功耗状态，内存保持供电。

**特点**:

- 保持内存供电
- 快速唤醒（1-2 秒）
- 仍然消耗少量电量
- 工作状态保持在内存中

**使用场景**:

- 短时间离开
- 快速恢复工作
- 笔记本电脑合盖

**示例**:

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "sleep"
    }
}
```

### 延迟执行

#### 立即执行

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown",
        "delay": 0
    }
}
```

或省略 delay 参数：

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown"
    }
}
```

#### 延迟执行

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown",
        "delay": 600,
        "message": "System will shutdown in 10 minutes"
    }
}
```

**延迟执行的优点**:

- 给用户时间保存工作
- 可以取消操作
- 显示倒计时通知

### 强制关闭

#### 正常关闭 (force=false)

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown",
        "force": false
    }
}
```

**行为**:

- 向所有应用程序发送关闭请求
- 允许应用程序保存数据
- 用户可以取消关闭（如果有未保存的数据）
- 更安全，推荐使用

#### 强制关闭 (force=true)

```json
{
    "name": "shutdown_system",
    "arguments": {
        "action": "shutdown",
        "force": true
    }
}
```

**行为**:

- 强制关闭所有应用程序
- 不等待应用程序响应
- 可能导致数据丢失
- 适用于紧急情况

**警告**: 强制关闭可能导致：

- 未保存的数据丢失
- 文件损坏
- 数据库事务中断

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "action": "shutdown",
    "delay": 60,
    "scheduled_time": "2026-02-04T12:11:00.000Z"
}
```

**字段说明**:

- `success`: 是否成功计划操作
- `action`: 电源操作类型
- `delay`: 延迟时间（秒）
- `scheduled_time`: 计划执行时间（ISO 8601 格式）

**错误响应**:

```json
{
    "error": "Insufficient privileges"
}
```

```json
{
    "error": "Hibernate is not supported on this system"
}
```

### 权限要求

电源管理操作需要 **SE_SHUTDOWN_NAME** 权限。

**获取权限**:

ClawDesk Server 会自动尝试启用此权限，但需要：

1. 以管理员身份运行
2. 用户账户具有关机权限

**检查权限**:

如果收到 "Insufficient privileges" 错误：

1. 右键点击 ClawDeskMCP.exe
2. 选择"以管理员身份运行"
3. 重试操作

### 审计日志示例

```json
{
    "time": "2026-02-04T12:00:00.000Z",
    "tool": "shutdown_system",
    "risk": "critical",
    "result": "ok",
    "duration_ms": 50,
    "high_risk": true,
    "details": {
        "action": "shutdown",
        "delay": 60,
        "force": false,
        "scheduled_time": "2026-02-04T12:01:00.000Z"
    }
}
```

---

## 取消关机

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "abort_shutdown",
    "arguments": {}
}
```

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/power/abort
```

### 参数说明

无参数。

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "message": "Shutdown cancelled successfully"
}
```

**字段说明**:

- `success`: 是否成功取消
- `message`: 操作结果消息

**错误响应**:

```json
{
    "error": "No shutdown is scheduled"
}
```

```json
{
    "error": "Insufficient privileges"
}
```

### 使用场景

#### 场景 1: 用户改变主意

用户计划关机后改变主意：

```bash
# 计划 5 分钟后关机
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "shutdown", "delay": 300}'

# 用户改变主意，取消关机
curl -X POST http://localhost:35182/power/abort
```

#### 场景 2: 发现未保存的工作

系统即将关机，用户发现有未保存的工作：

```bash
# 立即取消关机
curl -X POST http://localhost:35182/power/abort
```

### 限制

1. **只能取消通过 ClawDesk 计划的关机**: 无法取消通过其他方式（如 Windows 命令）计划的关机
2. **需要权限**: 需要 SE_SHUTDOWN_NAME 权限
3. **立即执行的操作无法取消**: delay=0 的操作会立即执行，无法取消

### 审计日志示例

```json
{
    "time": "2026-02-04T12:05:00.000Z",
    "tool": "abort_shutdown",
    "risk": "medium",
    "result": "ok",
    "duration_ms": 10,
    "details": {
        "message": "Shutdown cancelled successfully"
    }
}
```

---

## Dashboard 集成

### 关机倒计时显示

当计划关机或重启时，Dashboard 会显示倒计时横幅：

```
┌─────────────────────────────────────────────────────────┐
│ ⚠️  系统将在 5 分钟后关机                               │
│                                                         │
│ 倒计时: 04:58                                           │
│                                                         │
│ [取消关机]                                              │
└─────────────────────────────────────────────────────────┘
```

**功能**:

- 实时倒计时更新
- 显示操作类型（关机/重启）
- 一键取消按钮
- 红色警告样式

### 高风险操作标记

电源管理操作在 Dashboard 操作列表中用红色标记：

```
最近操作:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔴 shutdown_system    12:00:00    关机（延迟 60 秒）
🟢 list_processes     11:58:30    成功
🟢 read_file          11:57:15    成功
```

---

## 使用场景

### 场景 1: 定时关机

**需求**: 下载完成后自动关机

**解决方案**:

```bash
# 启动下载任务
# ...

# 计划 30 分钟后关机
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "shutdown", "delay": 1800, "message": "Download complete. System will shutdown in 30 minutes."}'
```

### 场景 2: 远程重启服务器

**需求**: 安装更新后远程重启服务器

**解决方案**:

```bash
# 安装更新
# ...

# 计划 2 分钟后重启
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "reboot", "delay": 120, "message": "Server will reboot in 2 minutes for updates."}'
```

### 场景 3: 节能管理

**需求**: 下班时自动进入休眠状态

**解决方案**:

```bash
# 每天 18:00 执行（通过定时任务）
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "hibernate"}'
```

### 场景 4: 紧急关机

**需求**: 系统出现问题，需要紧急关机

**解决方案**:

```bash
# 立即强制关机
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "shutdown", "delay": 0, "force": true}'
```

---

## 错误处理

### 常见错误

#### 1. 权限不足

**错误**: `Insufficient privileges`

**原因**: 没有 SE_SHUTDOWN_NAME 权限

**解决方案**:

1. 以管理员身份运行 ClawDesk Server
2. 确保用户账户有关机权限

#### 2. 休眠不支持

**错误**: `Hibernate is not supported on this system`

**原因**: 系统禁用了休眠功能

**解决方案**:

1. 以管理员身份打开命令提示符
2. 运行: `powercfg /hibernate on`
3. 重试操作

#### 3. 没有计划的关机

**错误**: `No shutdown is scheduled`

**原因**: 尝试取消不存在的关机计划

**解决方案**: 确认是否有计划的关机操作

#### 4. 延迟时间无效

**错误**: `Invalid delay value`

**原因**: delay 参数超出范围或为负数

**解决方案**: 使用 0 到 315360000 之间的值

---

## 最佳实践

### 1. 使用延迟执行

除非紧急情况，始终使用延迟执行，给用户时间保存工作：

```json
{
    "action": "shutdown",
    "delay": 60 // 至少 60 秒
}
```

### 2. 提供清晰的消息

使用 message 参数告知用户原因和时间：

```json
{
    "action": "shutdown",
    "delay": 300,
    "message": "System will shutdown in 5 minutes for maintenance. Please save your work."
}
```

### 3. 避免强制关闭

除非必要，不要使用 `force=true`：

```json
{
    "action": "shutdown",
    "force": false // 推荐
}
```

### 4. 监控审计日志

定期检查审计日志，了解电源管理操作历史。

### 5. 测试休眠功能

在生产环境使用前，先测试系统是否支持休眠。

### 6. 提供取消机制

在 UI 中提供明显的取消按钮，方便用户取消操作。

---

## 技术实现

### PowerService 类

```cpp
class PowerService {
public:
    PowerService(ConfigManager* configManager, PolicyGuard* policyGuard);

    // 执行电源操作
    ShutdownResult shutdownSystem(PowerAction action,
                                  int delay = 0,
                                  bool force = false,
                                  const std::string& message = "");

    // 取消计划的关机/重启
    AbortShutdownResult abortShutdown();

private:
    // 启用关机权限
    bool enableShutdownPrivilege();

    // 执行关机/重启
    bool initiateShutdown(PowerAction action, int delay, bool force, const std::string& message);

    // 执行休眠/睡眠
    bool initiateSuspend(PowerAction action);

    // 格式化计划时间
    std::string formatScheduledTime(int delay);
};
```

### 关键 API

#### Windows API

- `AdjustTokenPrivileges()`: 启用 SE_SHUTDOWN_NAME 权限
- `InitiateSystemShutdownEx()`: 关机/重启
- `AbortSystemShutdown()`: 取消关机
- `SetSuspendState()`: 休眠/睡眠

#### 权限启用

```cpp
bool PowerService::enableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // 打开进程令牌
    if (!OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                         &hToken)) {
        return false;
    }

    // 查找 SE_SHUTDOWN_NAME 权限
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 启用权限
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

    CloseHandle(hToken);
    return GetLastError() == ERROR_SUCCESS;
}
```

#### 关机/重启

```cpp
bool PowerService::initiateShutdown(PowerAction action, int delay, bool force, const std::string& message) {
    bool reboot = (action == PowerAction::Reboot);

    return InitiateSystemShutdownEx(
        NULL,                    // 本地计算机
        message.c_str(),         // 消息
        delay,                   // 延迟（秒）
        force,                   // 强制关闭应用程序
        reboot                   // 是否重启
    ) != 0;
}
```

#### 休眠/睡眠

```cpp
bool PowerService::initiateSuspend(PowerAction action) {
    bool hibernate = (action == PowerAction::Hibernate);

    return SetSuspendState(
        hibernate,               // TRUE=休眠, FALSE=睡眠
        FALSE,                   // 不强制
        FALSE                    // 不禁用唤醒事件
    ) != 0;
}
```

---

## 安全考虑

### 1. 关键风险操作

电源管理是 **Critical** 级别的风险操作：

- 需要用户确认（通过 PolicyGuard）
- 记录详细的审计日志
- Dashboard 显示红色警告

### 2. 权限检查

- 需要 SE_SHUTDOWN_NAME 权限
- 建议以管理员身份运行
- 权限不足时返回清晰的错误信息

### 3. 审计日志

所有电源管理操作都会记录：

- 时间戳
- 操作类型（关机/重启/休眠/睡眠）
- 延迟时间
- 是否强制
- 计划执行时间

### 4. 用户通知

- 延迟执行时显示系统通知
- Dashboard 显示倒计时
- 提供取消按钮

### 5. 配置控制

可以通过配置文件禁用电源管理功能：

```json
{
    "high_risk_operations": {
        "enable_power_management": false
    }
}
```

---

## 故障排除

### 问题 1: 无法关机

**症状**: 调用 shutdown_system 返回权限错误

**可能原因**:

1. 没有以管理员身份运行
2. 用户账户没有关机权限
3. 组策略限制

**解决步骤**:

1. 右键点击 ClawDeskMCP.exe，选择"以管理员身份运行"
2. 检查用户账户权限
3. 检查组策略设置（gpedit.msc）

### 问题 2: 休眠失败

**症状**: 调用 hibernate 返回不支持错误

**可能原因**:

1. 系统禁用了休眠功能
2. 磁盘空间不足
3. 驱动程序不支持

**解决步骤**:

1. 以管理员身份运行: `powercfg /hibernate on`
2. 检查磁盘空间（需要至少等于内存大小的空间）
3. 更新驱动程序

### 问题 3: 无法取消关机

**症状**: 调用 abort_shutdown 返回错误

**可能原因**:

1. 关机是通过其他方式计划的
2. 权限不足
3. 关机已经开始执行

**解决步骤**:

1. 确认关机是通过 ClawDesk 计划的
2. 以管理员身份运行
3. 如果关机已开始，可能无法取消

### 问题 4: 延迟时间不准确

**症状**: 实际关机时间与计划时间不符

**可能原因**:

1. 系统时间不准确
2. 时区设置问题
3. 系统负载过高

**解决步骤**:

1. 同步系统时间
2. 检查时区设置
3. 减少系统负载

---

## 与其他功能集成

### 与进程管理集成

在关机前终止特定进程：

```bash
# 终止占用资源的进程
curl -X POST http://localhost:35182/process/kill \
  -H "Content-Type: application/json" \
  -d '{"pid": 1234, "force": true}'

# 计划关机
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "shutdown", "delay": 60}'
```

### 与文件操作集成

在关机前备份重要文件：

```bash
# 备份文件
curl -X POST http://localhost:35182/file/copy \
  -H "Content-Type: application/json" \
  -d '{"source": "C:\\Users\\test\\important.txt", "destination": "C:\\Backup\\important.txt"}'

# 计划关机
curl -X POST http://localhost:35182/power/shutdown \
  -H "Content-Type: application/json" \
  -d '{"action": "shutdown", "delay": 120}'
```

---

## 参考资料

- [Windows Power Management API](https://docs.microsoft.com/en-us/windows/win32/power/power-management-functions)
- [InitiateSystemShutdownEx Function](https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-initiatesystemshutdownexa)
- [SetSuspendState Function](https://docs.microsoft.com/en-us/windows/win32/api/powrprof/nf-powrprof-setsuspendstate)
- [Shutdown Privileges](https://docs.microsoft.com/en-us/windows/win32/secauthz/privilege-constants)

---

## 版本历史

- v0.3.0 (2026-02-04)
    - ✅ 初始实现
    - ✅ 关机/重启功能
    - ✅ 休眠/睡眠功能
    - ✅ 延迟执行
    - ✅ 取消关机
    - ✅ SE_SHUTDOWN_NAME 权限管理
    - ✅ Dashboard 倒计时显示
    - ✅ 审计日志集成
