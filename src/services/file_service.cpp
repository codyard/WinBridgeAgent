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
#include "services/file_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
std::string toLower(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string trimTrailingSlash(const std::string& path) {
    if (path.size() > 3 && (path.back() == '\\' || path.back() == '/')) {
        return path.substr(0, path.size() - 1);
    }
    return path;
}

static bool pathExistsA(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

static std::string getDirNameA(const std::string& path) {
    if (path.empty()) return "";
    char buf[MAX_PATH] = {0};
    strncpy(buf, path.c_str(), MAX_PATH - 1);
    if (!PathRemoveFileSpecA(buf)) {
        return "";
    }
    return std::string(buf);
}

static std::string normalizeLineEndings(const std::string& input, const std::string& mode) {
    std::string m = toLower(mode);
    if (m.empty() || m == "auto") {
        // Keep as-is.
        return input;
    }
    if (m == "lf") {
        std::string out;
        out.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == '\r') {
                // Drop CR, preserve LF if present.
                continue;
            }
            out.push_back(c);
        }
        return out;
    }
    if (m == "crlf") {
        std::string out;
        out.reserve(input.size() + input.size() / 20);
        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            if (c == '\r') {
                // If already CRLF, keep CR and let next LF through.
                out.push_back('\r');
                continue;
            }
            if (c == '\n') {
                // If previous wasn't CR, add CR.
                if (i == 0 || input[i - 1] != '\r') {
                    out.push_back('\r');
                }
                out.push_back('\n');
                continue;
            }
            out.push_back(c);
        }
        return out;
    }
    // Unknown mode: keep as-is.
    return input;
}
} // namespace

FileService::FileService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

std::vector<FileInfo> FileService::findFiles(const FindFilesParams& params) {
    std::vector<FileInfo> results;
    if (!configManager_) {
        return results;
    }

    std::vector<std::string> allowedDirs = configManager_->getAllowedDirs();
    for (const auto& dir : allowedDirs) {
        if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
            break;
        }
        searchDirectory(dir, params, results);
    }

    return results;
}

std::vector<FileInfo> FileService::findFilesInPath(const std::string& path, const FindFilesParams& params) {
    std::vector<FileInfo> results;
    if (!configManager_) {
        return results;
    }
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        return results;
    }
    searchDirectory(path, params, results);
    return results;
}

std::string FileService::readTextFile(const std::string& path) {
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    int64_t size = getFileSize(path);
    if (size < 0) {
        throw std::runtime_error("File not found");
    }
    if (size > 200 * 1024) {
        throw std::runtime_error("File too large (max 200KB)");
    }
    if (!isTextFile(path)) {
        throw std::runtime_error("File type not allowed");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open file");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void FileService::writeTextFile(const std::string& path,
                                const std::string& content,
                                bool overwrite,
                                const std::string& lineEndings) {
    if (path.empty()) {
        throw std::runtime_error("Path required");
    }
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    if (!overwrite && pathExistsA(path)) {
        throw std::runtime_error("File already exists");
    }

    std::string dir = getDirNameA(path);
    if (!dir.empty()) {
        // Create intermediate directories if needed.
        SHCreateDirectoryExA(NULL, dir.c_str(), NULL);
    }

    std::string normalized = normalizeLineEndings(content, lineEndings);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for write");
    }
    out.write(normalized.data(), (std::streamsize)normalized.size());
    out.close();
    if (!out.good()) {
        throw std::runtime_error("Failed to write file");
    }
}

std::vector<SearchMatch> FileService::searchTextInFile(const std::string& path,
                                                       const std::string& query) {
    if (query.empty()) {
        throw std::runtime_error("Query required");
    }

    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    int64_t size = getFileSize(path);
    if (size < 0) {
        throw std::runtime_error("File not found");
    }
    if (size > 200 * 1024) {
        throw std::runtime_error("File too large (max 200KB)");
    }
    if (!isTextFile(path)) {
        throw std::runtime_error("File type not allowed");
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    std::vector<SearchMatch> matches;
    std::string line;
    int lineNumber = 0;
    std::string queryLower = toLower(query);
    while (std::getline(in, line)) {
        lineNumber++;
        std::string lineLower = toLower(line);
        if (lineLower.find(queryLower) != std::string::npos) {
            matches.push_back({lineNumber, line});
            if (matches.size() >= 200) {
                break;
            }
        }
    }
    return matches;
}

std::vector<DirectoryEntry> FileService::listDirectory(const std::string& path) {
    if (policyGuard_ && !policyGuard_->isPathAllowed(path)) {
        throw std::runtime_error("Path not allowed");
    }

    std::vector<DirectoryEntry> entries;
    std::string normalized = trimTrailingSlash(path);
    std::string searchPath = normalized + "\\*";

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Directory not found");
    }

    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        int64_t size = 0;
        if (!isDir) {
            LARGE_INTEGER li;
            li.HighPart = findData.nFileSizeHigh;
            li.LowPart = findData.nFileSizeLow;
            size = li.QuadPart;
        }

        DirectoryEntry entry;
        entry.name = name;
        entry.type = isDir ? "directory" : "file";
        entry.size = size;
        entry.modified = formatFileTime(findData.ftLastWriteTime);
        entries.push_back(entry);
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
    return entries;
}

bool FileService::isTextFile(const std::string& path) {
    std::string ext = toLower(getFileExtension(path));
    if (ext.empty()) {
        return true;
    }
    static const std::vector<std::string> allowed = {
        ".txt", ".log", ".json", ".md", ".csv", ".yaml", ".yml", ".ini", ".xml"
    };
    return std::find(allowed.begin(), allowed.end(), ext) != allowed.end();
}

int64_t FileService::getFileSize(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        return -1;
    }
    LARGE_INTEGER li;
    li.HighPart = data.nFileSizeHigh;
    li.LowPart = data.nFileSizeLow;
    return li.QuadPart;
}

std::string FileService::getFileExtension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    return path.substr(dot);
}

std::string FileService::formatFileTime(const FILETIME& ft) {
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) {
        return "";
    }
    char buffer[64];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

void FileService::searchDirectory(const std::string& dir,
                                  const FindFilesParams& params,
                                  std::vector<FileInfo>& results) {
    if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
        return;
    }

    std::string normalized = trimTrailingSlash(dir);
    std::string searchPath = normalized + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now;
    now.HighPart = nowFt.dwHighDateTime;
    now.LowPart = nowFt.dwLowDateTime;
    const uint64_t dayTicks = 24ULL * 60ULL * 60ULL * 10000000ULL;

    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        std::string fullPath = normalized + "\\" + name;
        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (isDir) {
            searchDirectory(fullPath, params, results);
            if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
                break;
            }
            continue;
        }

        if (!params.query.empty()) {
            if (toLower(name).find(toLower(params.query)) == std::string::npos) {
                continue;
            }
        }

        if (!params.exts.empty()) {
            std::string ext = toLower(getFileExtension(name));
            bool ok = false;
            for (const auto& allowed : params.exts) {
                if (ext == toLower(allowed)) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
        }

        if (params.days > 0) {
            ULARGE_INTEGER fileTime;
            fileTime.HighPart = findData.ftLastWriteTime.dwHighDateTime;
            fileTime.LowPart = findData.ftLastWriteTime.dwLowDateTime;
            uint64_t age = (now.QuadPart - fileTime.QuadPart) / dayTicks;
            if (age > static_cast<uint64_t>(params.days)) {
                continue;
            }
        }

        LARGE_INTEGER size;
        size.HighPart = findData.nFileSizeHigh;
        size.LowPart = findData.nFileSizeLow;

        FileInfo info;
        info.path = fullPath;
        info.size = size.QuadPart;
        info.modified = formatFileTime(findData.ftLastWriteTime);
        info.extension = getFileExtension(name);
        results.push_back(info);

        if (params.max > 0 && static_cast<int>(results.size()) >= params.max) {
            break;
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}
