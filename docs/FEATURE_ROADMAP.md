# ClawDesk MCP Server - 功能路线图

## 当前版本：v0.2.0

已实现功能：

- ✅ 基础 HTTP API 服务器
- ✅ Token 认证
- ✅ Dashboard 实时监控
- ✅ 文件读取、搜索、目录列表
- ✅ 剪贴板操作（文本、图片、文件）
- ✅ 截图功能
- ✅ 窗口列表
- ✅ 进程列表
- ✅ 命令执行
- ✅ MCP 协议基础实现

---

## 功能需求分析

### 优先级分类

#### 🔴 高优先级（v0.3.0）- 立即实施

解决实际使用中遇到的痛点问题

1. **进程管理增强**
    - ✅ 已有：进程列表
    - ⭐ 新增：终止进程（支持强制终止）
    - ⭐ 新增：进程优先级调整
    - 理由：解决无法关闭顽固进程的问题（如火绒）

2. **文件系统基础操作**
    - ✅ 已有：读取、搜索、列表
    - ⭐ 新增：删除文件/目录
    - ⭐ 新增：复制文件/目录
    - ⭐ 新增：移动/重命名文件
    - ⭐ 新增：创建目录
    - 理由：完善基础文件操作能力

3. **电源管理**
    - ⭐ 新增：关机（带倒计时）
    - ⭐ 新增：重启（带倒计时）
    - ⭐ 新增：休眠
    - ⭐ 新增：睡眠
    - ⭐ 新增：取消关机/重启
    - 理由：远程控制系统电源状态

#### 🟡 中优先级（v0.4.0）- 后续实施

增强系统管理能力

4. **服务管理**
    - 服务列表（名称、状态、启动类型）
    - 启动服务
    - 停止服务
    - 重启服务
    - 修改启动类型

5. **系统信息**
    - CPU 使用率（实时）
    - 内存使用情况
    - 磁盘 I/O 统计
    - 网络流量统计
    - 系统运行时间

6. **网络管理**
    - 网络连接列表
    - IP 配置信息
    - 网络适配器状态
    - 简单的网络测试（ping）

7. **文件搜索增强**
    - 按文件名搜索
    - 按文件类型搜索
    - 按修改时间搜索
    - 按文件大小搜索

#### 🟢 低优先级（v0.5.0+）- 长期规划

高级功能和系统维护

8. **注册表操作**
    - 读取注册表键值
    - 写入注册表键值（需谨慎）
    - 删除注册表键值
    - 枚举注册表子键

9. **事件日志**
    - 查看系统日志
    - 查看应用程序日志
    - 查看安全日志
    - 日志筛选和搜索

10. **防火墙管理**
    - 查看防火墙规则
    - 添加防火墙规则
    - 删除防火墙规则
    - 启用/禁用防火墙

11. **磁盘管理**
    - 磁盘清理（临时文件）
    - 磁盘碎片整理
    - 磁盘健康检查

12. **硬件控制**
    - 设备管理器信息
    - 启用/禁用设备
    - 音量控制
    - 显示器控制

#### ⚪ 暂不实施

安全风险较高或需求不明确

- ❌ 用户账户管理（安全风险高）
- ❌ 远程桌面控制（复杂度高）
- ❌ 软件安装/卸载（风险高）
- ❌ 杀毒扫描触发（依赖第三方）
- ❌ 系统备份（复杂度高）

---

## v0.3.0 详细规划

### 1. 进程管理增强

#### 1.1 终止进程

**端点**: `POST /process/kill`

**请求**:

```json
{
    "pid": 1234,
    "force": false
}
```

**参数**:

- `pid` (必需): 进程 ID
- `force` (可选): 是否强制终止，默认 false

**响应**:

```json
{
    "success": true,
    "pid": 1234,
    "name": "notepad.exe",
    "forced": false
}
```

**实现要点**:

- 使用 `OpenProcess` + `TerminateProcess`
- `force=false`: 先尝试 `WM_CLOSE`，失败后使用 `TerminateProcess`
- `force=true`: 直接使用 `TerminateProcess`
- 需要 `PROCESS_TERMINATE` 权限
- 记录到审计日志（高风险操作）

#### 1.2 调整进程优先级

**端点**: `POST /process/priority`

**请求**:

```json
{
    "pid": 1234,
    "priority": "normal"
}
```

**参数**:

- `pid` (必需): 进程 ID
- `priority` (必需): 优先级
    - `idle` - 空闲
    - `below_normal` - 低于正常
    - `normal` - 正常
    - `above_normal` - 高于正常
    - `high` - 高
    - `realtime` - 实时（需管理员权限）

**响应**:

```json
{
    "success": true,
    "pid": 1234,
    "old_priority": "normal",
    "new_priority": "high"
}
```

**实现要点**:

- 使用 `SetPriorityClass`
- 映射优先级常量
- 记录到审计日志

### 2. 文件系统操作

#### 2.1 删除文件/目录

**端点**: `DELETE /file`

**请求**:

```json
{
    "path": "C:\\temp\\file.txt",
    "recursive": false
}
```

**参数**:

- `path` (必需): 文件或目录路径
- `recursive` (可选): 是否递归删除目录，默认 false

**响应**:

```json
{
    "success": true,
    "path": "C:\\temp\\file.txt",
    "type": "file"
}
```

**实现要点**:

- 文件：使用 `DeleteFile`
- 目录：使用 `RemoveDirectory`（空目录）或递归删除
- 检查白名单
- 记录到审计日志（高风险操作）

#### 2.2 复制文件/目录

**端点**: `POST /file/copy`

**请求**:

```json
{
    "source": "C:\\temp\\file.txt",
    "destination": "C:\\backup\\file.txt",
    "overwrite": false
}
```

**响应**:

```json
{
    "success": true,
    "source": "C:\\temp\\file.txt",
    "destination": "C:\\backup\\file.txt",
    "size": 1024
}
```

**实现要点**:

- 使用 `CopyFile` 或 `CopyFileEx`
- 支持进度回调（可选）
- 检查白名单

#### 2.3 移动/重命名文件

**端点**: `POST /file/move`

**请求**:

```json
{
    "source": "C:\\temp\\old.txt",
    "destination": "C:\\temp\\new.txt"
}
```

**响应**:

```json
{
    "success": true,
    "source": "C:\\temp\\old.txt",
    "destination": "C:\\temp\\new.txt"
}
```

**实现要点**:

- 使用 `MoveFile` 或 `MoveFileEx`
- 检查白名单

#### 2.4 创建目录

**端点**: `POST /directory`

**请求**:

```json
{
    "path": "C:\\temp\\newdir",
    "recursive": true
}
```

**响应**:

```json
{
    "success": true,
    "path": "C:\\temp\\newdir"
}
```

**实现要点**:

- 使用 `CreateDirectory`
- `recursive=true`: 创建多级目录
- 检查白名单

### 3. 电源管理

#### 3.1 关机/重启

**端点**: `POST /power/shutdown`

**请求**:

```json
{
    "action": "shutdown",
    "delay": 60,
    "force": false,
    "message": "系统将在 60 秒后关机"
}
```

**参数**:

- `action` (必需): 操作类型
    - `shutdown` - 关机
    - `reboot` - 重启
    - `hibernate` - 休眠
    - `sleep` - 睡眠
- `delay` (可选): 延迟秒数，默认 0
- `force` (可选): 是否强制，默认 false
- `message` (可选): 显示给用户的消息

**响应**:

```json
{
    "success": true,
    "action": "shutdown",
    "delay": 60,
    "scheduled_time": "2026-02-03 23:00:00"
}
```

**实现要点**:

- 使用 `InitiateSystemShutdownEx` 或 `ExitWindowsEx`
- 需要 `SE_SHUTDOWN_NAME` 权限
- 记录到审计日志（高风险操作）
- Dashboard 显示倒计时

#### 3.2 取消关机/重启

**端点**: `POST /power/abort`

**响应**:

```json
{
    "success": true,
    "message": "Shutdown aborted"
}
```

**实现要点**:

- 使用 `AbortSystemShutdown`
- 需要 `SE_SHUTDOWN_NAME` 权限

---

## 实施计划

### 阶段 1：进程管理增强（1-2 天）

**任务**:

1. 实现 `/process/kill` 端点
2. 实现 `/process/priority` 端点
3. 添加审计日志记录
4. 编写测试脚本
5. 更新文档

**技术要点**:

- `OpenProcess(PROCESS_TERMINATE, ...)`
- `TerminateProcess()`
- `SetPriorityClass()`
- 错误处理（权限不足、进程不存在）

### 阶段 2：文件系统操作（2-3 天）

**任务**:

1. 实现 `DELETE /file` 端点
2. 实现 `POST /file/copy` 端点
3. 实现 `POST /file/move` 端点
4. 实现 `POST /directory` 端点
5. 白名单检查
6. 审计日志记录
7. 编写测试脚本
8. 更新文档

**技术要点**:

- `DeleteFile()`, `RemoveDirectory()`
- `CopyFile()`, `CopyFileEx()`
- `MoveFile()`, `MoveFileEx()`
- `CreateDirectory()`, `SHCreateDirectoryEx()`
- 递归目录操作

### 阶段 3：电源管理（1 天）

**任务**:

1. 实现 `/power/shutdown` 端点
2. 实现 `/power/abort` 端点
3. 权限提升处理
4. 审计日志记录
5. Dashboard 倒计时显示
6. 编写测试脚本
7. 更新文档

**技术要点**:

- `InitiateSystemShutdownEx()`
- `ExitWindowsEx()`
- `AbortSystemShutdown()`
- `AdjustTokenPrivileges()` (SE_SHUTDOWN_NAME)
- `SetSuspendState()` (休眠/睡眠)

---

## 安全考虑

### 高风险操作

以下操作需要特别注意安全性：

1. **进程终止**
    - 风险：可能终止系统关键进程
    - 防护：黑名单（禁止终止 system, csrss, winlogon 等）
    - 审计：记录所有终止操作

2. **文件删除**
    - 风险：误删重要文件
    - 防护：白名单检查、禁止删除系统目录
    - 审计：记录所有删除操作

3. **电源管理**
    - 风险：意外关机导致数据丢失
    - 防护：默认延迟、确认机制
    - 审计：记录所有电源操作

### 权限管理

- 所有操作都需要 Token 认证
- 高风险操作需要额外的确认参数
- 配置文件中可以禁用特定功能
- 审计日志记录所有操作

### 白名单/黑名单

**文件操作白名单**（config.json）:

```json
{
    "allowed_dirs": ["C:/Users", "C:/Temp", "D:/Data"],
    "forbidden_dirs": [
        "C:/Windows",
        "C:/Program Files",
        "C:/Program Files (x86)"
    ]
}
```

**进程终止黑名单**（硬编码）:

```cpp
const std::vector<std::string> PROTECTED_PROCESSES = {
    "system", "csrss.exe", "winlogon.exe", "services.exe",
    "lsass.exe", "smss.exe", "wininit.exe"
};
```

---

## MCP 工具映射

### 新增 MCP 工具

```json
{
    "tools": [
        {
            "name": "kill_process",
            "description": "Terminate a process by PID",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "pid": { "type": "number" },
                    "force": { "type": "boolean" }
                },
                "required": ["pid"]
            }
        },
        {
            "name": "set_process_priority",
            "description": "Set process priority",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "pid": { "type": "number" },
                    "priority": {
                        "type": "string",
                        "enum": [
                            "idle",
                            "below_normal",
                            "normal",
                            "above_normal",
                            "high",
                            "realtime"
                        ]
                    }
                },
                "required": ["pid", "priority"]
            }
        },
        {
            "name": "delete_file",
            "description": "Delete a file or directory",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "path": { "type": "string" },
                    "recursive": { "type": "boolean" }
                },
                "required": ["path"]
            }
        },
        {
            "name": "copy_file",
            "description": "Copy a file or directory",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "source": { "type": "string" },
                    "destination": { "type": "string" },
                    "overwrite": { "type": "boolean" }
                },
                "required": ["source", "destination"]
            }
        },
        {
            "name": "move_file",
            "description": "Move or rename a file",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "source": { "type": "string" },
                    "destination": { "type": "string" }
                },
                "required": ["source", "destination"]
            }
        },
        {
            "name": "create_directory",
            "description": "Create a directory",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "path": { "type": "string" },
                    "recursive": { "type": "boolean" }
                },
                "required": ["path"]
            }
        },
        {
            "name": "shutdown_system",
            "description": "Shutdown, reboot, hibernate or sleep the system",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "action": {
                        "type": "string",
                        "enum": ["shutdown", "reboot", "hibernate", "sleep"]
                    },
                    "delay": { "type": "number" },
                    "force": { "type": "boolean" },
                    "message": { "type": "string" }
                },
                "required": ["action"]
            }
        },
        {
            "name": "abort_shutdown",
            "description": "Cancel a scheduled shutdown or reboot",
            "inputSchema": {
                "type": "object",
                "properties": {}
            }
        }
    ]
}
```

---

## 测试计划

### 单元测试

- 进程终止（正常进程、受保护进程）
- 文件操作（白名单内、白名单外）
- 电源管理（延迟、取消）

### 集成测试

- 完整的 API 调用流程
- 错误处理和恢复
- 审计日志记录

### 安全测试

- 权限检查
- 白名单/黑名单验证
- Token 认证

---

## 文档更新

需要更新的文档：

1. **README.md** - 添加新功能说明
2. **docs/API.md** - 详细的 API 文档
3. **docs/MCP.md** - MCP 工具说明
4. **CHANGELOG.md** - 版本更新日志
5. **FEATURES.md** - 功能列表
6. **新增：docs/ProcessManagement.md** - 进程管理指南
7. **新增：docs/FileOperations.md** - 文件操作指南
8. **新增：docs/PowerManagement.md** - 电源管理指南

---

## 版本发布计划

### v0.3.0 - 系统控制增强版

**预计发布**: 2026-02-10

**主要功能**:

- ✅ 进程终止和优先级调整
- ✅ 文件系统基础操作（删除、复制、移动、创建目录）
- ✅ 电源管理（关机、重启、休眠、睡眠）

**改进**:

- 增强的安全机制（白名单、黑名单）
- 完善的审计日志
- Dashboard 显示高风险操作

### v0.4.0 - 系统管理增强版

**预计发布**: 2026-02-20

**主要功能**:

- 服务管理
- 系统信息监控
- 网络管理
- 文件搜索增强

### v0.5.0 - 高级功能版

**预计发布**: 2026-03-01

**主要功能**:

- 注册表操作
- 事件日志查看
- 防火墙管理
- 磁盘管理

---

## 总结

**v0.3.0 重点**：

1. 🔴 进程管理增强 - 解决实际痛点
2. 🔴 文件系统操作 - 完善基础能力
3. 🔴 电源管理 - 远程控制能力

**预计工作量**：4-6 天
**风险等级**：中等（需要仔细处理安全问题）
**用户价值**：高（解决实际使用中的问题）

这个规划平衡了功能需求、安全性和实施难度，优先实现最有价值的功能。
