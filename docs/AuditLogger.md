# AuditLogger 模块文档

## 概述

`AuditLogger` 是 ClawDesk MCP Server 的审计日志模块，负责记录所有工具调用的详细信息，包括时间戳、工具名称、风险等级、执行结果和耗时。日志采用 JSON Lines 格式存储，支持线程安全的并发写入、自动日志轮转和过期日志清理。

## 功能特性

- **JSON Lines 格式**：每行一个 JSON 对象，便于解析和分析
- **UTC ISO 8601 时间戳**：统一使用 UTC 时区，格式为 `YYYY-MM-DDTHH:MM:SS.sssZ`
- **线程安全**：使用互斥锁保护并发写入
- **自动日志轮转**：按日期自动创建新日志文件（`audit-YYYY-MM-DD.log`）
- **过期日志清理**：根据保留天数自动删除旧日志
- **统计功能**：支持查询指定天数内的工具调用统计

## 类接口

### AuditLogger

```cpp
class AuditLogger {
public:
    // 构造函数：初始化日志记录器
    explicit AuditLogger(const std::string& logPath);

    // 析构函数：确保文件关闭
    ~AuditLogger();

    // 记录工具调用
    void logToolCall(const AuditLogEntry& entry);

    // 清理过期日志（根据保留天数）
    void cleanupOldLogs(int retentionDays);

    // 获取最近 N 天的统计数据
    std::map<std::string, int> getStats(int days);
};
```

### AuditLogEntry

```cpp
struct AuditLogEntry {
    std::string time;           // UTC ISO 8601 时间戳
    std::string tool;           // 工具名称
    RiskLevel risk;             // 风险等级
    std::string result;         // 结果状态："ok", "error", "denied"
    int duration_ms;            // 执行耗时（毫秒）
    std::string error;          // 错误信息（可选）
    bool high_risk;             // v0.3.0: 高风险操作标记（默认 false）
    nlohmann::json details;     // v0.3.0: 操作详情（JSON 格式，可选）
};
```

### RiskLevel

```cpp
enum class RiskLevel {
    Low,        // 低风险
    Medium,     // 中风险
    High,       // 高风险
    Critical    // 严重风险
};
```

## 使用示例

### 基本使用

```cpp
#include "support/audit_logger.h"

// 创建日志记录器
AuditLogger logger("logs/audit.log");

// 记录成功的工具调用
AuditLogEntry entry;
entry.time = "2026-02-03T12:03:01.234Z";
entry.tool = "screenshot_full";
entry.risk = RiskLevel::High;
entry.result = "ok";
entry.duration_ms = 128;
entry.error = "";

logger.logToolCall(entry);
```

### 记录错误

```cpp
// 记录失败的工具调用
AuditLogEntry errorEntry;
errorEntry.time = "2026-02-03T12:05:30.567Z";
errorEntry.tool = "run_command_restricted";
errorEntry.risk = RiskLevel::Critical;
errorEntry.result = "error";
errorEntry.duration_ms = 5;
errorEntry.error = "Command not in whitelist";

logger.logToolCall(errorEntry);
```

### 清理过期日志

```cpp
// 清理 7 天前的日志（免费版）
logger.cleanupOldLogs(7);

// 清理 90 天前的日志（付费版）
logger.cleanupOldLogs(90);
```

### 获取统计数据

```cpp
// 获取最近 7 天的工具调用统计
auto stats = logger.getStats(7);

for (const auto& [tool, count] : stats) {
    std::cout << tool << ": " << count << " calls" << std::endl;
}
```

### 记录高风险操作（v0.3.0）

```cpp
// 记录进程终止操作
AuditLogEntry killEntry;
killEntry.time = "2026-02-03T12:00:00.000Z";
killEntry.tool = "kill_process";
killEntry.risk = RiskLevel::High;
killEntry.result = "ok";
killEntry.duration_ms = 15;
killEntry.high_risk = true;
killEntry.details = nlohmann::json{
    {"pid", 1234},
    {"process_name", "notepad.exe"},
    {"forced", false}
};

logger.logToolCall(killEntry);

// 记录文件删除操作
AuditLogEntry deleteEntry;
deleteEntry.time = "2026-02-03T12:05:00.000Z";
deleteEntry.tool = "delete_file";
deleteEntry.risk = RiskLevel::High;
deleteEntry.result = "ok";
deleteEntry.duration_ms = 230;
deleteEntry.high_risk = true;
deleteEntry.details = nlohmann::json{
    {"path", "C:/Users/test/temp/file.txt"},
    {"type", "file"},
    {"recursive", false}
};

logger.logToolCall(deleteEntry);

// 记录电源管理操作
AuditLogEntry shutdownEntry;
shutdownEntry.time = "2026-02-03T12:10:00.000Z";
shutdownEntry.tool = "shutdown_system";
shutdownEntry.risk = RiskLevel::Critical;
shutdownEntry.result = "ok";
shutdownEntry.duration_ms = 50;
shutdownEntry.high_risk = true;
shutdownEntry.details = nlohmann::json{
    {"action", "shutdown"},
    {"delay", 60},
    {"force", false},
    {"scheduled_time", "2026-02-03T12:11:00.000Z"}
};

logger.logToolCall(shutdownEntry);
```

## 日志格式

### 日志文件命名

日志文件按日期自动轮转，命名格式为：

```
audit-YYYY-MM-DD.log
```

例如：

- `audit-2026-02-03.log`
- `audit-2026-02-04.log`

### JSON Lines 格式

每行一个 JSON 对象，示例：

```json
{"time":"2026-02-03T12:03:01.234Z","tool":"screenshot_full","risk":"high","result":"ok","duration_ms":128}
{"time":"2026-02-03T12:05:30.567Z","tool":"find_files","risk":"medium","result":"ok","duration_ms":45}
{"time":"2026-02-03T12:10:15.890Z","tool":"run_command_restricted","risk":"critical","result":"denied","duration_ms":2}
{"time":"2026-02-03T12:15:20.123Z","tool":"read_text_file","risk":"medium","result":"error","duration_ms":8,"error":"File not in whitelist"}
```

**v0.3.0 高风险操作日志示例：**

```json
{"time":"2026-02-03T12:00:00.000Z","tool":"kill_process","risk":"high","result":"ok","duration_ms":15,"high_risk":true,"details":{"pid":1234,"process_name":"notepad.exe","forced":false}}
{"time":"2026-02-03T12:05:00.000Z","tool":"delete_file","risk":"high","result":"ok","duration_ms":230,"high_risk":true,"details":{"path":"C:/Users/test/temp/file.txt","type":"file","recursive":false}}
{"time":"2026-02-03T12:10:00.000Z","tool":"shutdown_system","risk":"critical","result":"ok","duration_ms":50,"high_risk":true,"details":{"action":"shutdown","delay":60,"force":false,"scheduled_time":"2026-02-03T12:11:00.000Z"}}
```

### 字段说明

| 字段          | 类型   | 必填 | 说明                                            |
| ------------- | ------ | ---- | ----------------------------------------------- |
| `time`        | string | 是   | UTC ISO 8601 时间戳                             |
| `tool`        | string | 是   | 工具名称                                        |
| `risk`        | string | 是   | 风险等级：`low`, `medium`, `high`, `critical`   |
| `result`      | string | 是   | 执行结果：`ok`, `error`, `denied`               |
| `duration_ms` | number | 是   | 执行耗时（毫秒）                                |
| `error`       | string | 否   | 错误信息（仅在 `result` 为 `error` 时存在）     |
| `high_risk`   | bool   | 否   | v0.3.0: 高风险操作标记（仅在为 `true` 时存在）  |
| `details`     | object | 否   | v0.3.0: 操作详情（JSON 对象，仅在有详情时存在） |

## 线程安全

`AuditLogger` 使用互斥锁（`std::mutex`）保护所有写入操作，确保多线程环境下的安全性：

```cpp
void AuditLogger::logToolCall(const AuditLogEntry& entry) {
    std::lock_guard<std::mutex> lock(logMutex_);
    // ... 写入操作
}
```

## 日志轮转

日志文件按日期自动轮转：

1. 每次写入日志时，检查当前日期是否变化
2. 如果日期变化，关闭当前日志文件，打开新日志文件
3. 新日志文件名包含新日期：`audit-YYYY-MM-DD.log`

## 过期日志清理

`cleanupOldLogs()` 方法根据保留天数自动删除旧日志：

- **免费版**：保留 7 天（需求 21.4）
- **付费版**：保留 90 天（需求 21.3）

清理逻辑：

1. 遍历日志目录中的所有文件
2. 检查文件名是否匹配 `audit-*.log` 模式
3. 比较文件修改时间与保留期限
4. 删除超过保留期限的日志文件

## 统计功能

`getStats()` 方法统计指定天数内的工具调用次数：

```cpp
auto stats = logger.getStats(7);
// 返回：{"screenshot_full": 15, "find_files": 23, "clipboard_read": 8, ...}
```

统计逻辑：

1. 遍历日志目录中的所有日志文件
2. 过滤出指定天数内的日志文件
3. 解析每行 JSON，统计每个工具的调用次数
4. 返回工具名称到调用次数的映射

## 性能考虑

- **即时写入**：每次调用 `logToolCall()` 后立即 `flush()`，确保日志不丢失
- **文件追加模式**：使用 `std::ios::app` 模式打开文件，避免覆盖
- **互斥锁开销**：写入操作较快，锁竞争开销可忽略
- **日志文件大小**：按日期轮转，单个文件不会过大

## 错误处理

- **目录不存在**：自动创建日志目录
- **文件打开失败**：输出错误信息到 `stderr`，但不抛出异常
- **JSON 解析失败**：跳过格式错误的日志行
- **文件删除失败**：输出错误信息，继续处理其他文件

## 需求覆盖

| 需求 | 说明                             | 实现                    |
| ---- | -------------------------------- | ----------------------- |
| 13.1 | 记录所有工具调用                 | `logToolCall()`         |
| 13.2 | JSON Lines 格式                  | 每行一个 JSON 对象      |
| 13.3 | 包含时间、工具、风险、结果、耗时 | `AuditLogEntry` 结构    |
| 13.4 | UTC ISO 8601 时间戳              | `getCurrentTimestamp()` |
| 13.5 | 持久化到本地文件                 | `std::ofstream`         |
| 21.3 | 付费版保留 90 天                 | `cleanupOldLogs(90)`    |
| 21.4 | 免费版保留 7 天                  | `cleanupOldLogs(7)`     |

## 测试

单元测试位于 `tests/unit/test_audit_logger.cpp`，包括：

1. **基本日志记录**：验证日志文件创建和 JSON 格式
2. **多条日志**：验证多次写入
3. **线程安全**：验证并发写入
4. **错误日志**：验证错误字段
5. **风险等级转换**：验证枚举到字符串的转换
6. **统计功能**：验证工具调用统计

运行测试：

```bash
./build-tests.sh
# 在 Windows 上运行：
wine build-tests/test_audit_logger.exe
```

## 集成示例

在主程序中集成 `AuditLogger`：

```cpp
#include "support/audit_logger.h"
#include "support/config_manager.h"
#include "support/license_manager.h"

int main() {
    // 初始化配置管理器
    ConfigManager configManager("config.json");
    configManager.load();

    // 初始化许可证管理器
    LicenseManager licenseManager(&configManager);

    // 初始化审计日志
    AuditLogger auditLogger("logs/audit.log");

    // 定期清理过期日志
    auto licenseInfo = licenseManager.validateLicense();
    int retentionDays = (licenseInfo.status == LicenseStatus::Active) ? 90 : 7;
    auditLogger.cleanupOldLogs(retentionDays);

    // 工具调用时记录日志
    auto startTime = std::chrono::steady_clock::now();

    // ... 执行工具 ...

    auto endTime = std::chrono::steady_clock::now();
    int durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime
    ).count();

    AuditLogEntry entry;
    entry.time = getCurrentUTCTimestamp();  // 需要实现此辅助函数
    entry.tool = "screenshot_full";
    entry.risk = RiskLevel::High;
    entry.result = "ok";
    entry.duration_ms = durationMs;
    entry.error = "";

    auditLogger.logToolCall(entry);

    return 0;
}
```

## 注意事项

1. **时区**：所有时间戳必须使用 UTC 时区
2. **文件权限**：确保日志目录有写入权限
3. **磁盘空间**：定期清理过期日志，避免磁盘占满
4. **并发安全**：多线程环境下自动加锁，无需额外处理
5. **日志轮转**：每天自动创建新日志文件，无需手动干预

## 未来改进

- [ ] 支持日志压缩（gzip）
- [ ] 支持日志大小限制（单文件最大 100MB）
- [ ] 支持异步写入（提高性能）
- [ ] 支持日志查询 API（按时间范围、工具名称过滤）
- [ ] 支持日志导出（CSV、Excel）
