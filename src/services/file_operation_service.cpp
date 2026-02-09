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
#include "services/file_operation_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shlwapi.h>
#include <algorithm>
#include <cctype>
#include <iostream>

// Link with Shlwapi.lib for path functions
#pragma comment(lib, "Shlwapi.lib")

// ===== 静态成员初始化 =====

const std::vector<std::string> FileOperationService::FORBIDDEN_DIRS = {
    "c:\\windows",
    "c:\\program files",
    "c:\\program files (x86)",
    "c:\\programdata\\microsoft"
};

// ===== 辅助函数 =====

// 字符串转小写
static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// 检查字符串是否以指定前缀开始
static bool startsWith(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }
    return str.compare(0, prefix.length(), prefix) == 0;
}

// ===== 构造函数 =====

FileOperationService::FileOperationService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

// ===== 公共方法 =====

DeleteFileResult FileOperationService::deleteFile(const std::string& path, bool recursive) {
    DeleteFileResult result;
    result.path = path;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        args["path"] = path;
        args["recursive"] = recursive;
        auto decision = policyGuard_->evaluateToolCall("delete_file", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 检查路径是否在白名单中
    if (!isPathAllowed(path)) {
        result.error = "Path not allowed: " + path;
        return result;
    }

    // 2. 检查是否为系统目录
    if (isSystemDirectory(path)) {
        result.error = "System directory protected: " + path;
        return result;
    }

    // 3. 检查路径是否存在
    if (!pathExists(path)) {
        result.error = "Path does not exist: " + path;
        return result;
    }

    // 4. 判断是文件还是目录
    if (isDirectory(path)) {
        result.type = "directory";
        
        if (recursive) {
            // 递归删除目录
            if (deleteDirectoryRecursive(path)) {
                result.success = true;
                std::cout << "Directory deleted recursively: " << path << std::endl;
            } else {
                result.error = "Failed to delete directory recursively";
            }
        } else {
            // 仅删除空目录
            if (RemoveDirectoryA(path.c_str())) {
                result.success = true;
                std::cout << "Empty directory deleted: " << path << std::endl;
            } else {
                DWORD error = GetLastError();
                if (error == ERROR_DIR_NOT_EMPTY) {
                    result.error = "Directory is not empty (use recursive=true to delete)";
                } else {
                    result.error = "Failed to delete directory (error code: " + std::to_string(error) + ")";
                }
            }
        }
    } else {
        result.type = "file";
        
        // 删除文件
        if (DeleteFileA(path.c_str())) {
            result.success = true;
            std::cout << "File deleted: " << path << std::endl;
        } else {
            DWORD error = GetLastError();
            result.error = "Failed to delete file (error code: " + std::to_string(error) + ")";
        }
    }

    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("delete_file");
    }
    return result;
}

CopyFileResult FileOperationService::copyFile(const std::string& source,
                                             const std::string& destination,
                                             bool overwrite) {
    CopyFileResult result;
    result.source = source;
    result.destination = destination;
    result.size = 0;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        args["source"] = source;
        args["destination"] = destination;
        args["overwrite"] = overwrite;
        auto decision = policyGuard_->evaluateToolCall("copy_file", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 检查源路径和目标路径是否在白名单中
    if (!isPathAllowed(source)) {
        result.error = "Source path not allowed: " + source;
        return result;
    }
    if (!isPathAllowed(destination)) {
        result.error = "Destination path not allowed: " + destination;
        return result;
    }

    // 2. 检查源路径是否存在
    if (!pathExists(source)) {
        result.error = "Source path does not exist: " + source;
        return result;
    }

    // 3. 判断是文件还是目录
    if (isDirectory(source)) {
        // 递归复制目录
        if (copyDirectoryRecursive(source, destination, overwrite)) {
            result.success = true;
            result.size = 0; // 目录大小不计算
            std::cout << "Directory copied: " << source << " -> " << destination << std::endl;
        } else {
            result.error = "Failed to copy directory";
        }
    } else {
        // 复制文件
        BOOL failIfExists = !overwrite;
        if (CopyFileA(source.c_str(), destination.c_str(), failIfExists)) {
            result.success = true;
            result.size = getFileSize(destination);
            std::cout << "File copied: " << source << " -> " << destination 
                     << " (" << result.size << " bytes)" << std::endl;
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_EXISTS) {
                result.error = "Destination file already exists (use overwrite=true to replace)";
            } else {
                result.error = "Failed to copy file (error code: " + std::to_string(error) + ")";
            }
        }
    }

    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("copy_file");
    }
    return result;
}

MoveFileResult FileOperationService::moveFile(const std::string& source,
                                             const std::string& destination) {
    MoveFileResult result;
    result.source = source;
    result.destination = destination;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        args["source"] = source;
        args["destination"] = destination;
        auto decision = policyGuard_->evaluateToolCall("move_file", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 检查源路径和目标路径是否在白名单中
    if (!isPathAllowed(source)) {
        result.error = "Source path not allowed: " + source;
        return result;
    }
    if (!isPathAllowed(destination)) {
        result.error = "Destination path not allowed: " + destination;
        return result;
    }

    // 2. 检查源路径是否存在
    if (!pathExists(source)) {
        result.error = "Source path does not exist: " + source;
        return result;
    }

    // 3. 移动文件（带覆盖标志）
    if (MoveFileExA(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        result.success = true;
        std::cout << "File moved: " << source << " -> " << destination << std::endl;
    } else {
        DWORD error = GetLastError();
        result.error = "Failed to move file (error code: " + std::to_string(error) + ")";
    }

    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("move_file");
    }
    return result;
}

CreateDirectoryResult FileOperationService::createDirectory(const std::string& path, bool recursive) {
    CreateDirectoryResult result;
    result.path = path;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        args["path"] = path;
        args["recursive"] = recursive;
        auto decision = policyGuard_->evaluateToolCall("create_directory", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 检查路径是否在白名单中
    if (!isPathAllowed(path)) {
        result.error = "Path not allowed: " + path;
        return result;
    }

    // 2. 如果目录已存在，返回成功
    if (pathExists(path) && isDirectory(path)) {
        result.success = true;
        std::cout << "Directory already exists: " << path << std::endl;
        return result;
    }

    // 3. 创建目录
    if (recursive) {
        // 递归创建多级目录
        if (createDirectoryRecursive(path)) {
            result.success = true;
            std::cout << "Directory created recursively: " << path << std::endl;
        } else {
            result.error = "Failed to create directory recursively";
        }
    } else {
        // 仅创建单级目录
        if (CreateDirectoryA(path.c_str(), NULL)) {
            result.success = true;
            std::cout << "Directory created: " << path << std::endl;
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_PATH_NOT_FOUND) {
                result.error = "Parent directory does not exist (use recursive=true to create)";
            } else if (error == ERROR_ALREADY_EXISTS) {
                result.success = true; // 目录已存在，视为成功
            } else {
                result.error = "Failed to create directory (error code: " + std::to_string(error) + ")";
            }
        }
    }

    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("create_directory");
    }
    return result;
}

// ===== 私有方法 =====

bool FileOperationService::isPathAllowed(const std::string& path) {
    if (!configManager_) {
        return false;
    }

    std::string normalizedPath = normalizePath(path);
    std::vector<std::string> allowedDirs = configManager_->getAllowedDirs();

    for (const auto& allowedDir : allowedDirs) {
        std::string normalizedAllowedDir = normalizePath(allowedDir);
        if (startsWith(normalizedPath, normalizedAllowedDir)) {
            return true;
        }
    }

    return false;
}

bool FileOperationService::isSystemDirectory(const std::string& path) {
    std::string normalizedPath = normalizePath(path);

    for (const auto& forbiddenDir : FORBIDDEN_DIRS) {
        if (startsWith(normalizedPath, forbiddenDir)) {
            return true;
        }
    }

    return false;
}

bool FileOperationService::deleteDirectoryRecursive(const std::string& path) {
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        std::string fileName = findData.cFileName;
        
        // 跳过 "." 和 ".."
        if (fileName == "." || fileName == "..") {
            continue;
        }

        std::string fullPath = path + "\\" + fileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // 递归删除子目录
            if (!deleteDirectoryRecursive(fullPath)) {
                FindClose(hFind);
                return false;
            }
        } else {
            // 删除文件
            if (!DeleteFileA(fullPath.c_str())) {
                FindClose(hFind);
                return false;
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // 删除目录本身
    return RemoveDirectoryA(path.c_str()) != 0;
}

bool FileOperationService::copyDirectoryRecursive(const std::string& source,
                                                  const std::string& destination,
                                                  bool overwrite) {
    // 1. 创建目标目录
    if (!CreateDirectoryA(destination.c_str(), NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }

    // 2. 遍历源目录
    WIN32_FIND_DATAA findData;
    std::string searchPath = source + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool success = true;

    do {
        std::string fileName = findData.cFileName;
        
        // 跳过 "." 和 ".."
        if (fileName == "." || fileName == "..") {
            continue;
        }

        std::string sourcePath = source + "\\" + fileName;
        std::string destPath = destination + "\\" + fileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // 递归复制子目录
            if (!copyDirectoryRecursive(sourcePath, destPath, overwrite)) {
                success = false;
                break;
            }
        } else {
            // 复制文件
            BOOL failIfExists = !overwrite;
            if (!CopyFileA(sourcePath.c_str(), destPath.c_str(), failIfExists)) {
                success = false;
                break;
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return success;
}

bool FileOperationService::createDirectoryRecursive(const std::string& path) {
    // 如果目录已存在，返回成功
    if (pathExists(path)) {
        return isDirectory(path);
    }

    // 查找最后一个路径分隔符
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash == std::string::npos) {
        // 没有父目录，直接创建
        return CreateDirectoryA(path.c_str(), NULL) != 0;
    }

    // 递归创建父目录
    std::string parentPath = path.substr(0, lastSlash);
    if (!parentPath.empty() && !pathExists(parentPath)) {
        if (!createDirectoryRecursive(parentPath)) {
            return false;
        }
    }

    // 创建当前目录
    if (CreateDirectoryA(path.c_str(), NULL)) {
        return true;
    }

    // 如果目录已存在，也视为成功
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

int64_t FileOperationService::getFileSize(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
        return -1;
    }

    LARGE_INTEGER size;
    size.HighPart = fileInfo.nFileSizeHigh;
    size.LowPart = fileInfo.nFileSizeLow;
    return size.QuadPart;
}

std::string FileOperationService::normalizePath(const std::string& path) {
    std::string normalized = path;
    
    // 将所有斜杠转换为反斜杠
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    
    // 转换为小写
    normalized = toLower(normalized);
    
    // 移除末尾的反斜杠（除非是根目录）
    if (normalized.length() > 3 && normalized.back() == '\\') {
        normalized.pop_back();
    }
    
    return normalized;
}

bool FileOperationService::pathExists(const std::string& path) {
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
}

bool FileOperationService::isDirectory(const std::string& path) {
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
