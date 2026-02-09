# 文件系统操作 - File Operations

## 概述

ClawDesk MCP Server v0.3.0 提供了完整的文件系统操作功能，包括删除、复制、移动和创建目录。所有操作都经过严格的安全检查，确保只能在授权的目录中进行操作。

## 功能列表

### 1. 删除文件或目录 (delete_file)

删除指定的文件或目录，支持递归删除。

### 2. 复制文件或目录 (copy_file)

复制文件或目录到目标位置。

### 3. 移动或重命名文件 (move_file)

移动文件到新位置或重命名文件。

### 4. 创建目录 (create_directory)

创建新目录，支持多级目录创建。

---

## 删除文件或目录

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "delete_file",
    "arguments": {
        "path": "C:\\Users\\test\\temp\\file.txt",
        "recursive": false
    }
}
```

**HTTP API 调用**:

```bash
curl -X DELETE http://localhost:35182/file \
  -H "Content-Type: application/json" \
  -d '{"path": "C:\\Users\\test\\temp\\file.txt", "recursive": false}'
```

### 参数说明

- **path** (string, 必需): 文件或目录路径
    - 必须使用绝对路径
    - Windows 路径使用反斜杠 `\` 或正斜杠 `/`
    - 必须在 `allowed_dirs` 白名单中
- **recursive** (boolean, 可选): 是否递归删除目录，默认 false
    - `false`: 只能删除空目录
    - `true`: 递归删除目录及其所有内容

### 删除模式

#### 删除文件

```json
{
    "name": "delete_file",
    "arguments": {
        "path": "C:\\Users\\test\\document.txt"
    }
}
```

**行为**:

- 直接删除文件
- 不需要 recursive 参数

#### 删除空目录

```json
{
    "name": "delete_file",
    "arguments": {
        "path": "C:\\Users\\test\\empty_folder",
        "recursive": false
    }
}
```

**行为**:

- 只能删除空目录
- 如果目录非空，返回错误

#### 递归删除目录

```json
{
    "name": "delete_file",
    "arguments": {
        "path": "C:\\Users\\test\\project",
        "recursive": true
    }
}
```

**行为**:

- 递归删除目录及其所有子目录和文件
- **警告**: 此操作不可恢复！

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "path": "C:\\Users\\test\\temp\\file.txt",
    "type": "file"
}
```

**字段说明**:

- `success`: 是否成功删除
- `path`: 删除的路径
- `type`: 类型（"file" 或 "directory"）

**错误响应**:

```json
{
    "error": "Path not allowed"
}
```

```json
{
    "error": "System directory protected"
}
```

```json
{
    "error": "Directory not empty (use recursive=true)"
}
```

### 安全限制

#### 1. 白名单检查

路径必须在 `config.json` 的 `allowed_dirs` 中：

```json
{
    "allowed_dirs": [
        "C:/Users/YourUsername/Downloads",
        "C:/Users/YourUsername/Documents",
        "C:/Users/YourUsername/Desktop"
    ]
}
```

**检查规则**:

- 路径前缀必须匹配白名单中的某一项
- 支持子目录（例如白名单包含 `C:/Users/test`，则 `C:/Users/test/subfolder` 也允许）

#### 2. 系统目录保护

以下目录受到硬编码保护，**无法删除**：

| 目录                     | 说明                 |
| ------------------------ | -------------------- |
| C:\Windows               | Windows 系统目录     |
| C:\Program Files         | 程序安装目录（64位） |
| C:\Program Files (x86)   | 程序安装目录（32位） |
| C:\ProgramData\Microsoft | Microsoft 应用数据   |

**设计原因**:

- 这些目录包含系统关键文件
- 删除它们会导致系统无法启动
- 硬编码在 `FileOperationService::FORBIDDEN_DIRS` 中
- 不可通过配置文件修改

### 审计日志示例

```json
{
    "time": "2026-02-04T11:00:00.000Z",
    "tool": "delete_file",
    "risk": "high",
    "result": "ok",
    "duration_ms": 230,
    "high_risk": true,
    "details": {
        "path": "C:\\Users\\test\\temp\\file.txt",
        "type": "file",
        "recursive": false
    }
}
```

---

## 复制文件或目录

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "copy_file",
    "arguments": {
        "source": "C:\\Users\\test\\file.txt",
        "destination": "C:\\Users\\test\\backup\\file.txt",
        "overwrite": false
    }
}
```

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/file/copy \
  -H "Content-Type: application/json" \
  -d '{"source": "C:\\Users\\test\\file.txt", "destination": "C:\\Users\\test\\backup\\file.txt", "overwrite": false}'
```

### 参数说明

- **source** (string, 必需): 源路径
    - 必须存在
    - 必须在 `allowed_dirs` 白名单中
- **destination** (string, 必需): 目标路径
    - 必须在 `allowed_dirs` 白名单中
    - 父目录必须存在
- **overwrite** (boolean, 可选): 是否覆盖已存在的文件，默认 false
    - `false`: 如果目标文件存在，返回错误
    - `true`: 覆盖目标文件

### 复制模式

#### 复制文件

```json
{
    "name": "copy_file",
    "arguments": {
        "source": "C:\\Users\\test\\document.txt",
        "destination": "C:\\Users\\test\\backup\\document.txt"
    }
}
```

**行为**:

- 复制单个文件
- 保留文件属性和时间戳

#### 复制目录

```json
{
    "name": "copy_file",
    "arguments": {
        "source": "C:\\Users\\test\\project",
        "destination": "C:\\Users\\test\\backup\\project"
    }
}
```

**行为**:

- 递归复制整个目录树
- 保留目录结构
- 复制所有子目录和文件

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "source": "C:\\Users\\test\\file.txt",
    "destination": "C:\\Users\\test\\backup\\file.txt",
    "size": 1024
}
```

**字段说明**:

- `success`: 是否成功复制
- `source`: 源路径
- `destination`: 目标路径
- `size`: 文件大小（字节）

**错误响应**:

```json
{
    "error": "Destination file already exists (use overwrite=true)"
}
```

```json
{
    "error": "Source path not allowed"
}
```

```json
{
    "error": "Destination path not allowed"
}
```

### 安全限制

1. **双重白名单检查**: 源路径和目标路径都必须在白名单中
2. **覆盖保护**: 默认不覆盖已存在的文件
3. **审计日志**: 所有复制操作都会记录

### 审计日志示例

```json
{
    "time": "2026-02-04T11:05:00.000Z",
    "tool": "copy_file",
    "risk": "medium",
    "result": "ok",
    "duration_ms": 450,
    "details": {
        "source": "C:\\Users\\test\\file.txt",
        "destination": "C:\\Users\\test\\backup\\file.txt",
        "size": 1024,
        "overwrite": false
    }
}
```

---

## 移动或重命名文件

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "move_file",
    "arguments": {
        "source": "C:\\Users\\test\\old.txt",
        "destination": "C:\\Users\\test\\new.txt"
    }
}
```

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/file/move \
  -H "Content-Type: application/json" \
  -d '{"source": "C:\\Users\\test\\old.txt", "destination": "C:\\Users\\test\\new.txt"}'
```

### 参数说明

- **source** (string, 必需): 源路径
    - 必须存在
    - 必须在 `allowed_dirs` 白名单中
- **destination** (string, 必需): 目标路径
    - 必须在 `allowed_dirs` 白名单中
    - 如果目标文件存在，会被覆盖

### 使用场景

#### 重命名文件

```json
{
    "name": "move_file",
    "arguments": {
        "source": "C:\\Users\\test\\old_name.txt",
        "destination": "C:\\Users\\test\\new_name.txt"
    }
}
```

#### 移动文件到其他目录

```json
{
    "name": "move_file",
    "arguments": {
        "source": "C:\\Users\\test\\Downloads\\file.txt",
        "destination": "C:\\Users\\test\\Documents\\file.txt"
    }
}
```

#### 移动并重命名

```json
{
    "name": "move_file",
    "arguments": {
        "source": "C:\\Users\\test\\Downloads\\old.txt",
        "destination": "C:\\Users\\test\\Documents\\new.txt"
    }
}
```

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "source": "C:\\Users\\test\\old.txt",
    "destination": "C:\\Users\\test\\new.txt"
}
```

**字段说明**:

- `success`: 是否成功移动
- `source`: 源路径
- `destination`: 目标路径

**错误响应**:

```json
{
    "error": "Source file not found"
}
```

```json
{
    "error": "Source path not allowed"
}
```

```json
{
    "error": "Destination path not allowed"
}
```

### 注意事项

1. **源文件会被删除**: 移动操作会删除源文件
2. **跨驱动器移动**: 如果源和目标在不同驱动器，会先复制再删除
3. **覆盖行为**: 如果目标文件存在，会被覆盖（无需 overwrite 参数）

### 审计日志示例

```json
{
    "time": "2026-02-04T11:10:00.000Z",
    "tool": "move_file",
    "risk": "medium",
    "result": "ok",
    "duration_ms": 120,
    "details": {
        "source": "C:\\Users\\test\\old.txt",
        "destination": "C:\\Users\\test\\new.txt"
    }
}
```

---

## 创建目录

### 基本用法

**MCP 工具调用**:

```json
{
    "name": "create_directory",
    "arguments": {
        "path": "C:\\Users\\test\\new_folder",
        "recursive": false
    }
}
```

**HTTP API 调用**:

```bash
curl -X POST http://localhost:35182/directory \
  -H "Content-Type: application/json" \
  -d '{"path": "C:\\Users\\test\\new_folder", "recursive": false}'
```

### 参数说明

- **path** (string, 必需): 目录路径
    - 必须在 `allowed_dirs` 白名单中
- **recursive** (boolean, 可选): 是否创建多级目录，默认 false
    - `false`: 只能创建单级目录，父目录必须存在
    - `true`: 自动创建所有不存在的父目录

### 创建模式

#### 创建单级目录

```json
{
    "name": "create_directory",
    "arguments": {
        "path": "C:\\Users\\test\\new_folder",
        "recursive": false
    }
}
```

**前提条件**: `C:\Users\test` 必须存在

#### 创建多级目录

```json
{
    "name": "create_directory",
    "arguments": {
        "path": "C:\\Users\\test\\level1\\level2\\level3",
        "recursive": true
    }
}
```

**行为**: 自动创建 `level1`、`level2`、`level3` 三级目录

### 响应格式

**成功响应**:

```json
{
    "success": true,
    "path": "C:\\Users\\test\\new_folder"
}
```

**字段说明**:

- `success`: 是否成功创建
- `path`: 创建的目录路径

**错误响应**:

```json
{
    "error": "Parent directory does not exist (use recursive=true)"
}
```

```json
{
    "error": "Path not allowed"
}
```

### 特殊情况

#### 目录已存在

如果目录已经存在，操作仍然返回成功：

```json
{
    "success": true,
    "path": "C:\\Users\\test\\existing_folder"
}
```

这是设计行为，确保幂等性。

### 审计日志示例

```json
{
    "time": "2026-02-04T11:15:00.000Z",
    "tool": "create_directory",
    "risk": "low",
    "result": "ok",
    "duration_ms": 25,
    "details": {
        "path": "C:\\Users\\test\\new_folder",
        "recursive": false
    }
}
```

---

## 使用场景

### 场景 1: 清理临时文件

**需求**: 删除下载目录中的临时文件

**解决方案**:

```bash
# 列出临时文件
curl "http://localhost:35182/list?path=C:\\Users\\test\\Downloads"

# 删除单个文件
curl -X DELETE http://localhost:35182/file \
  -H "Content-Type: application/json" \
  -d '{"path": "C:\\Users\\test\\Downloads\\temp.txt"}'
```

### 场景 2: 备份重要文件

**需求**: 将文档目录备份到另一个位置

**解决方案**:

```bash
# 创建备份目录
curl -X POST http://localhost:35182/directory \
  -H "Content-Type: application/json" \
  -d '{"path": "C:\\Users\\test\\Backup", "recursive": true}'

# 复制文件
curl -X POST http://localhost:35182/file/copy \
  -H "Content-Type: application/json" \
  -d '{"source": "C:\\Users\\test\\Documents\\important.txt", "destination": "C:\\Users\\test\\Backup\\important.txt"}'
```

### 场景 3: 整理文件结构

**需求**: 将文件移动到新的目录结构

**解决方案**:

```bash
# 创建新目录结构
curl -X POST http://localhost:35182/directory \
  -H "Content-Type: application/json" \
  -d '{"path": "C:\\Users\\test\\Organized\\2026\\02", "recursive": true}'

# 移动文件
curl -X POST http://localhost:35182/file/move \
  -H "Content-Type: application/json" \
  -d '{"source": "C:\\Users\\test\\Downloads\\report.pdf", "destination": "C:\\Users\\test\\Organized\\2026\\02\\report.pdf"}'
```

---

## 错误处理

### 常见错误

#### 1. 路径不在白名单中

**错误**: `Path not allowed`

**原因**: 路径不在 `config.json` 的 `allowed_dirs` 中

**解决方案**:

1. 编辑 `config.json`
2. 添加路径到 `allowed_dirs`
3. 重启 ClawDesk Server 或重新加载配置

#### 2. 系统目录保护

**错误**: `System directory protected`

**原因**: 尝试删除受保护的系统目录

**解决方案**: 这是设计行为，无法绕过

#### 3. 目录非空

**错误**: `Directory not empty (use recursive=true)`

**原因**: 尝试删除非空目录但未设置 `recursive=true`

**解决方案**: 设置 `recursive=true` 或先删除目录中的文件

#### 4. 文件已存在

**错误**: `Destination file already exists (use overwrite=true)`

**原因**: 复制时目标文件已存在且未设置 `overwrite=true`

**解决方案**: 设置 `overwrite=true` 或先删除目标文件

---

## 最佳实践

### 1. 始终使用绝对路径

避免使用相对路径，确保操作的是正确的文件。

### 2. 谨慎使用递归删除

递归删除操作不可恢复，删除前请确认路径正确。

### 3. 备份重要文件

在删除或移动重要文件前，先创建备份。

### 4. 配置合理的白名单

只将需要操作的目录添加到白名单，最小化风险。

### 5. 监控审计日志

定期检查审计日志，了解文件操作历史。

### 6. 使用 overwrite 参数

复制文件时明确指定是否覆盖，避免意外覆盖。

---

## 技术实现

### FileOperationService 类

```cpp
class FileOperationService {
public:
    FileOperationService(ConfigManager* configManager, PolicyGuard* policyGuard);

    // 删除文件或目录
    DeleteFileResult deleteFile(const std::string& path, bool recursive);

    // 复制文件或目录
    CopyFileResult copyFile(const std::string& source,
                           const std::string& destination,
                           bool overwrite);

    // 移动或重命名文件
    MoveFileResult moveFile(const std::string& source,
                           const std::string& destination);

    // 创建目录
    CreateDirectoryResult createDirectory(const std::string& path, bool recursive);

private:
    // 检查路径是否在白名单中
    bool isPathAllowed(const std::string& path);

    // 检查是否为系统目录
    bool isSystemDirectory(const std::string& path);

    // 禁止操作的系统目录列表
    static const std::vector<std::string> FORBIDDEN_DIRS;
};
```

### 关键 API

- `DeleteFile()`: 删除文件
- `RemoveDirectory()`: 删除空目录
- `FindFirstFile()` / `FindNextFile()`: 遍历目录
- `CopyFile()` / `CopyFileEx()`: 复制文件
- `MoveFile()` / `MoveFileEx()`: 移动文件
- `CreateDirectory()`: 创建目录
- `SHCreateDirectoryEx()`: 创建多级目录

---

## 安全考虑

### 1. 白名单机制

所有路径都必须经过白名单检查，确保只能操作授权的目录。

### 2. 系统目录保护

硬编码的禁止目录列表，防止误删系统文件。

### 3. 用户确认

高风险操作（delete_file）需要用户通过 MessageBox 确认。

### 4. 审计日志

所有文件操作都会记录详细信息：

- 时间戳
- 操作类型
- 源路径和目标路径
- 操作结果
- 文件大小

### 5. 错误处理

完善的错误处理机制，防止操作失败导致系统不稳定。

---

## 故障排除

### 问题 1: 无法删除文件

**症状**: 调用 delete_file 返回错误

**可能原因**:

1. 路径不在白名单中
2. 文件被其他进程占用
3. 权限不足

**解决步骤**:

1. 检查 `config.json` 的 `allowed_dirs`
2. 关闭占用文件的进程
3. 以管理员身份运行 ClawDesk Server

### 问题 2: 复制大文件很慢

**症状**: 复制大文件时响应时间很长

**可能原因**:

1. 文件太大
2. 磁盘 I/O 性能限制
3. 跨驱动器复制

**解决步骤**:

1. 考虑使用异步操作（未来版本）
2. 检查磁盘性能
3. 避免跨驱动器复制大文件

### 问题 3: 递归删除失败

**症状**: 递归删除目录时部分文件未删除

**可能原因**:

1. 某些文件被占用
2. 权限不足
3. 文件属性（只读、隐藏）

**解决步骤**:

1. 检查审计日志查看详细错误
2. 关闭占用文件的进程
3. 修改文件属性

---

## 参考资料

- [Windows File Management API](https://docs.microsoft.com/en-us/windows/win32/fileio/file-management-functions)
- [CopyFile Function](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-copyfile)
- [MoveFile Function](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefile)
- [DeleteFile Function](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefile)

---

## 版本历史

- v0.3.0 (2026-02-04)
    - ✅ 初始实现
    - ✅ 文件删除（单文件 + 递归目录）
    - ✅ 文件复制（单文件 + 递归目录）
    - ✅ 文件移动/重命名
    - ✅ 目录创建（单级 + 多级）
    - ✅ 白名单检查
    - ✅ 系统目录保护
    - ✅ 审计日志集成
