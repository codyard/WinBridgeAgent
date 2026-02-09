# Dashboard v0.3.0 新功能说明

## 概述

v0.3.0 版本为 Dashboard 添加了高风险操作监控和关机倒计时功能，增强了系统安全性和用户体验。

## 新增功能

### 1. 高风险操作标记

#### 功能描述

Dashboard 现在会特别标记高风险操作，使用 `[!!!]` 标记，便于用户快速识别潜在危险操作。

#### 高风险操作列表

- **kill_process** - 终止进程
- **delete_file** - 删除文件或目录（递归删除）
- **shutdown_system** - 系统关机/重启

#### 显示格式

```
[18:35:20.123] [!!!] kill_process - HIGH RISK OPERATION
      {"pid": 1234, "process_name": "notepad.exe", "forced": false}
```

#### 实现细节

- 日志条目新增 `highRisk` 字段（bool 类型）
- 高风险操作使用特殊的类型标记 `high_risk`
- 格式化时显示为 `[!!!]` 标记

### 2. 高风险操作计数器

#### 功能描述

Dashboard 右上角显示高风险操作的累计执行次数，帮助用户了解系统的安全操作频率。

#### 显示位置

```
┌─────────────────────────────────────────────────────────────┐
│  Status: Total logs: 45 (max 1000)  High-Risk Ops: 3       │
└─────────────────────────────────────────────────────────────┘
```

#### 实现细节

- 使用 `DashboardState.highRiskOperationCount` 字段存储计数
- 每次调用 `logHighRiskOperation()` 时自动递增
- 通过 `updateHighRiskCounter()` 方法更新显示
- 服务器重启后重置为 0

### 3. 关机倒计时横幅

#### 功能描述

当系统计划关机或重启时，Dashboard 顶部会显示红色警告横幅，实时显示剩余时间。

#### 显示效果

```
┌─────────────────────────────────────────────────────────────┐
│ [WARNING: System shutdown scheduled in 60 seconds] [Cancel]│
└─────────────────────────────────────────────────────────────┘
```

#### 触发条件

- 调用 `shutdown_system` 工具且 `delay > 0`
- 调用 `showShutdownCountdown(action, remainingSeconds)` 方法

#### 实现细节

- 使用 `DashboardState` 存储关机状态：
    - `shutdownScheduled` - 是否已计划关机
    - `shutdownAction` - 操作类型（shutdown/reboot/hibernate/sleep）
    - `shutdownDelay` - 延迟秒数
    - `shutdownTime` - 计划执行时间（chrono::time_point）
- 倒计时标签初始隐藏，计划关机时显示
- 实时更新剩余秒数

### 4. 取消关机按钮

#### 功能描述

倒计时横幅右侧显示 "Cancel" 按钮，用户可以快速请求取消关机。

#### 行为

1. 点击 "Cancel" 按钮
2. 隐藏倒计时横幅
3. 记录取消操作日志
4. 显示提示对话框，建议使用 `abort_shutdown` 工具确认

#### 实现细节

- 按钮 ID: `ID_CANCEL_SHUTDOWN_BUTTON`
- 处理函数: `handleCancelShutdown()`
- 注意：Dashboard 不直接调用 PowerService，需要通过 MCP 工具完成实际取消

## API 接口

### 新增方法

#### logHighRiskOperation

```cpp
void logHighRiskOperation(const std::string& tool, const std::string& details);
```

记录高风险操作日志。

**参数：**

- `tool` - 工具名称（如 "kill_process"）
- `details` - 操作详情（JSON 格式字符串）

**示例：**

```cpp
dashboard->logHighRiskOperation("kill_process",
    R"({"pid": 1234, "process_name": "notepad.exe", "forced": false})");
```

#### showShutdownCountdown

```cpp
void showShutdownCountdown(const std::string& action, int remainingSeconds);
```

显示关机倒计时横幅。

**参数：**

- `action` - 操作类型（"shutdown", "reboot", "hibernate", "sleep"）
- `remainingSeconds` - 剩余秒数

**示例：**

```cpp
dashboard->showShutdownCountdown("shutdown", 60);
```

#### hideShutdownCountdown

```cpp
void hideShutdownCountdown();
```

隐藏关机倒计时横幅。

**示例：**

```cpp
dashboard->hideShutdownCountdown();
```

#### updateHighRiskCounter

```cpp
void updateHighRiskCounter(int count);
```

更新高风险操作计数器显示。

**参数：**

- `count` - 当前计数

**示例：**

```cpp
dashboard->updateHighRiskCounter(5);
```

## 数据结构

### DashboardLogEntry

```cpp
struct DashboardLogEntry {
    std::string timestamp;      // 时间戳
    std::string type;           // 类型: "request", "processing", "success", "error", "high_risk"
    std::string tool;           // 工具名称
    std::string message;        // 消息内容
    std::string details;        // 详细信息（可选）
    bool highRisk;              // v0.3.0: 是否为高风险操作
};
```

### DashboardState

```cpp
struct DashboardState {
    bool shutdownScheduled;                             // 是否已计划关机
    std::string shutdownAction;                         // 操作类型
    int shutdownDelay;                                  // 延迟秒数
    std::chrono::system_clock::time_point shutdownTime; // 计划执行时间
    int highRiskOperationCount;                         // 高风险操作计数
};
```

## 集成指南

### 在服务中使用

#### 1. 记录高风险操作

在 ProcessService、FileOperationService、PowerService 中：

```cpp
// 终止进程后
if (dashboard && dashboard->isVisible()) {
    nlohmann::json details;
    details["pid"] = pid;
    details["process_name"] = processName;
    details["forced"] = force;
    dashboard->logHighRiskOperation("kill_process", details.dump());
}
```

#### 2. 显示关机倒计时

在 PowerService 的 `shutdownSystem()` 方法中：

```cpp
if (delay > 0 && dashboard && dashboard->isVisible()) {
    dashboard->showShutdownCountdown(actionStr, delay);
}
```

#### 3. 隐藏倒计时

在 PowerService 的 `abortShutdown()` 方法中：

```cpp
if (dashboard && dashboard->isVisible()) {
    dashboard->hideShutdownCountdown();
}
```

## 测试方法

### 测试高风险操作标记

1. 启动服务器并打开 Dashboard
2. 执行高风险操作（如 kill_process）
3. 观察日志中的 `[!!!]` 标记
4. 检查右上角计数器是否递增

### 测试关机倒计时

1. 启动服务器并打开 Dashboard
2. 调用 `shutdown_system` 工具，设置 `delay=60`
3. 观察顶部倒计时横幅
4. 点击 "Cancel" 按钮
5. 验证横幅隐藏

### 测试脚本

参考 `../scripts/test_dashboard.sh` 脚本进行自动化测试。

## 注意事项

### 1. 线程安全

所有 Dashboard 方法都是线程安全的，使用 `std::mutex` 保护共享数据。

### 2. 性能影响

- 只有在 Dashboard 可见时才更新 UI
- 隐藏 Dashboard 不影响日志记录
- 高风险操作标记不增加显著开销

### 3. 倒计时更新

当前实现中，倒计时显示在调用 `showShutdownCountdown()` 时设置，不会自动更新。如需实时倒计时，需要：

- 使用定时器（WM_TIMER）定期调用 `updateCountdownDisplay()`
- 或在主消息循环中定期更新

### 4. 取消关机限制

Dashboard 的 "Cancel" 按钮只是发送请求，实际取消需要：

- 调用 `abort_shutdown` MCP 工具
- 或在 PowerService 中实现回调机制

## 相关文档

- [Dashboard.md](Dashboard.md) - Dashboard 主要文档
- [PowerManagement.md](PowerManagement.md) - 电源管理文档
- [ProcessManagement.md](ProcessManagement.md) - 进程管理文档
- [FileOperations.md](FileOperations.md) - 文件操作文档

## 版本历史

- **v0.3.0** (2026-02-03)
    - 新增高风险操作标记
    - 新增高风险操作计数器
    - 新增关机倒计时横幅
    - 新增取消关机按钮
