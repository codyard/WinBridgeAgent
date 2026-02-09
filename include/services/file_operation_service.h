/*
 * Copyright (C) 2026 Codyard
 *
 * This file is part of WinBridgeAgent.
 *
 * WinBridgeAgent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinBridgeAgent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinBridgeAgent. If not, see <https://www.gnu.org/licenses/\>.
 */
#ifndef FILE_OPERATION_SERVICE_H
#define FILE_OPERATION_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>

// Forward declarations
class ConfigManager;
class PolicyGuard;

/**
 * DeleteFileResult - 文件删除结果
 * 
 * 包含文件删除操作的结果信息：
 * - success: 操作是否成功
 * - path: 被删除的路径
 * - type: 类型（"file" 或 "directory"）
 * - error: 错误信息（如果失败）
 */
struct DeleteFileResult {
    bool success;
    std::string path;
    std::string type;
    std::string error;
};

/**
 * CopyFileResult - 文件复制结果
 * 
 * 包含文件复制操作的结果信息：
 * - success: 操作是否成功
 * - source: 源路径
 * - destination: 目标路径
 * - size: 文件大小（字节）
 * - error: 错误信息（如果失败）
 */
struct CopyFileResult {
    bool success;
    std::string source;
    std::string destination;
    int64_t size;
    std::string error;
};

/**
 * MoveFileResult - 文件移动结果
 * 
 * 包含文件移动/重命名操作的结果信息：
 * - success: 操作是否成功
 * - source: 源路径
 * - destination: 目标路径
 * - error: 错误信息（如果失败）
 */
struct MoveFileResult {
    bool success;
    std::string source;
    std::string destination;
    std::string error;
};

/**
 * CreateDirectoryResult - 目录创建结果
 * 
 * 包含目录创建操作的结果信息：
 * - success: 操作是否成功
 * - path: 创建的目录路径
 * - error: 错误信息（如果失败）
 */
struct CreateDirectoryResult {
    bool success;
    std::string path;
    std::string error;
};

/**
 * FileOperationService - 文件系统操作服务
 * 
 * 提供文件和目录的删除、复制、移动、创建功能。
 * 
 * 功能：
 * - 删除文件或目录（单文件 + 递归目录）
 * - 复制文件或目录（单文件 + 递归目录）
 * - 移动或重命名文件
 * - 创建目录（单级 + 多级）
 * - 白名单检查
 * - 系统目录保护
 * 
 * 需求覆盖：
 * - 27.1: 删除指定的文件或目录
 * - 27.2: 仅允许删除白名单目录中的文件
 * - 27.3: 禁止删除系统目录
 * - 27.4: 递归删除目录及其所有内容
 * - 27.5: 非空目录且 recursive=false 时返回错误
 * - 27.6: 记录操作到审计日志
 * - 28.1: 复制文件或目录到目标位置
 * - 28.2: 移动或重命名文件
 * - 28.3: 验证源路径和目标路径都在白名单中
 * - 28.4: overwrite=false 且目标文件存在时返回错误
 * - 28.5: overwrite=true 时覆盖目标文件
 * - 28.6: 返回源路径、目标路径和文件大小
 * - 29.1: 创建指定的目录
 * - 29.2: 仅允许在白名单目录中创建目录
 * - 29.3: recursive=true 时创建多级目录
 * - 29.4: recursive=false 且父目录不存在时返回错误
 * - 29.5: 目录已存在时返回成功状态
 */
class FileOperationService {
public:
    /**
     * 构造函数
     * @param configManager 配置管理器指针
     * @param policyGuard 策略守卫指针（可选，可为 nullptr）
     */
    FileOperationService(ConfigManager* configManager, PolicyGuard* policyGuard);

    /**
     * 删除文件或目录
     * 
     * @param path 要删除的路径
     * @param recursive 是否递归删除（对目录有效）
     * @return DeleteFileResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 检查路径是否在白名单中
     * 2. 检查是否为系统目录
     * 3. 判断是文件还是目录
     * 4. 如果是文件，使用 DeleteFile 删除
     * 5. 如果是目录：
     *    - recursive=true: 递归删除所有内容
     *    - recursive=false: 仅删除空目录（RemoveDirectory）
     * 6. 返回结果
     */
    DeleteFileResult deleteFile(const std::string& path, bool recursive);

    /**
     * 复制文件或目录
     * 
     * @param source 源路径
     * @param destination 目标路径
     * @param overwrite 是否覆盖已存在的文件
     * @return CopyFileResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 检查源路径和目标路径是否在白名单中
     * 2. 检查源路径是否存在
     * 3. 判断是文件还是目录
     * 4. 如果是文件，使用 CopyFile 复制
     * 5. 如果是目录，递归复制所有内容
     * 6. 返回结果（包含文件大小）
     */
    CopyFileResult copyFile(const std::string& source,
                           const std::string& destination,
                           bool overwrite);

    /**
     * 移动或重命名文件
     * 
     * @param source 源路径
     * @param destination 目标路径
     * @return MoveFileResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 检查源路径和目标路径是否在白名单中
     * 2. 检查源路径是否存在
     * 3. 使用 MoveFileEx 移动文件（带 MOVEFILE_REPLACE_EXISTING 标志）
     * 4. 返回结果
     */
    MoveFileResult moveFile(const std::string& source,
                           const std::string& destination);

    /**
     * 创建目录
     * 
     * @param path 要创建的目录路径
     * @param recursive 是否创建多级目录
     * @return CreateDirectoryResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 检查路径是否在白名单中
     * 2. 如果目录已存在，返回成功
     * 3. 如果 recursive=true，递归创建所有父目录
     * 4. 如果 recursive=false，仅创建单级目录（CreateDirectory）
     * 5. 返回结果
     */
    CreateDirectoryResult createDirectory(const std::string& path, bool recursive);

private:
    /**
     * 检查路径是否在白名单中
     * 
     * 验证路径前缀是否在 allowed_dirs 配置中
     * 
     * @param path 要检查的路径
     * @return true 表示路径在白名单中
     */
    bool isPathAllowed(const std::string& path);

    /**
     * 检查是否为系统目录
     * 
     * 系统目录列表（硬编码）：
     * - C:\Windows
     * - C:\Program Files
     * - C:\Program Files (x86)
     * - C:\ProgramData\Microsoft
     * 
     * @param path 要检查的路径
     * @return true 表示是系统目录
     */
    bool isSystemDirectory(const std::string& path);

    /**
     * 递归删除目录
     * 
     * 实现逻辑：
     * 1. 使用 FindFirstFile + FindNextFile 遍历目录
     * 2. 递归删除所有子目录
     * 3. 删除所有文件
     * 4. 最后删除目录本身
     * 
     * @param path 目录路径
     * @return true 表示删除成功
     */
    bool deleteDirectoryRecursive(const std::string& path);

    /**
     * 递归复制目录
     * 
     * 实现逻辑：
     * 1. 创建目标目录
     * 2. 使用 FindFirstFile + FindNextFile 遍历源目录
     * 3. 递归复制所有子目录
     * 4. 复制所有文件
     * 
     * @param source 源目录路径
     * @param destination 目标目录路径
     * @param overwrite 是否覆盖已存在的文件
     * @return true 表示复制成功
     */
    bool copyDirectoryRecursive(const std::string& source,
                               const std::string& destination,
                               bool overwrite);

    /**
     * 创建多级目录
     * 
     * 实现逻辑：
     * 1. 分割路径为各级目录
     * 2. 从根目录开始，逐级创建
     * 3. 如果某级目录已存在，继续下一级
     * 
     * @param path 目录路径
     * @return true 表示创建成功
     */
    bool createDirectoryRecursive(const std::string& path);

    /**
     * 获取文件大小
     * 
     * @param path 文件路径
     * @return 文件大小（字节），失败返回 -1
     */
    int64_t getFileSize(const std::string& path);

    /**
     * 规范化路径
     * 
     * 将路径转换为统一格式（反斜杠、小写）
     * 
     * @param path 原始路径
     * @return 规范化后的路径
     */
    std::string normalizePath(const std::string& path);

    /**
     * 检查路径是否存在
     * 
     * @param path 路径
     * @return true 表示路径存在
     */
    bool pathExists(const std::string& path);

    /**
     * 检查是否为目录
     * 
     * @param path 路径
     * @return true 表示是目录
     */
    bool isDirectory(const std::string& path);

    /**
     * 禁止操作的系统目录列表（硬编码）
     * 
     * 这些目录不允许进行删除操作，以保护系统稳定性
     */
    static const std::vector<std::string> FORBIDDEN_DIRS;

    ConfigManager* configManager_;
    PolicyGuard* policyGuard_;
};

#endif // FILE_OPERATION_SERVICE_H
