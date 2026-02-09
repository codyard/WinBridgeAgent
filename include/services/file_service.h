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
#ifndef CLAWDESK_FILE_SERVICE_H
#define CLAWDESK_FILE_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

class ConfigManager;
class PolicyGuard;

struct FindFilesParams {
    std::string query;
    std::vector<std::string> exts;
    int days;
    int max;
};

struct FileInfo {
    std::string path;
    int64_t size;
    std::string modified;
    std::string extension;
};

struct DirectoryEntry {
    std::string name;
    std::string type;
    int64_t size;
    std::string modified;
};

struct SearchMatch {
    int line;
    std::string text;
};

class FileService {
public:
    FileService(ConfigManager* configManager, PolicyGuard* policyGuard);

    std::vector<FileInfo> findFiles(const FindFilesParams& params);
    std::vector<FileInfo> findFilesInPath(const std::string& path, const FindFilesParams& params);
    std::string readTextFile(const std::string& path);
    void writeTextFile(const std::string& path,
                       const std::string& content,
                       bool overwrite,
                       const std::string& lineEndings);
    std::vector<SearchMatch> searchTextInFile(const std::string& path, const std::string& query);
    std::vector<DirectoryEntry> listDirectory(const std::string& path);

private:
    bool isTextFile(const std::string& path);
    int64_t getFileSize(const std::string& path);
    std::string getFileExtension(const std::string& path);
    std::string formatFileTime(const FILETIME& ft);
    void searchDirectory(const std::string& dir,
                         const FindFilesParams& params,
                         std::vector<FileInfo>& results);

    ConfigManager* configManager_;
    PolicyGuard* policyGuard_;
};

#endif // CLAWDESK_FILE_SERVICE_H
