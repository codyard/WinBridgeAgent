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
#include "utils/log_path.h"
#include <shlobj.h>

namespace clawdesk {

static std::wstring GetExeDirW() {
    wchar_t path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (len == 0) return L"";
    std::wstring full(path, len);
    size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    return full.substr(0, pos);
}

std::wstring GetLogDirW() {
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &appData)) && appData) {
        std::wstring base(appData);
        CoTaskMemFree(appData);
        return base + L"\\WinBridgeAgent\\logs";
    }
    std::wstring dir = GetExeDirW();
    if (dir.empty()) return L"logs";
    return dir + L"\\logs";
}

void EnsureLogDir() {
    std::wstring logDir = GetLogDirW();
    size_t pos = logDir.rfind(L'\\');
    if (pos != std::wstring::npos) {
        std::wstring parentDir = logDir.substr(0, pos);
        CreateDirectoryW(parentDir.c_str(), NULL);
    }
    CreateDirectoryW(logDir.c_str(), NULL);
}

std::wstring GetLogFilePathW(const wchar_t* filename) {
    return GetLogDirW() + L"\\" + filename;
}

std::string GetLogFilePathA(const char* filename) {
    std::wstring wpath = GetLogFilePathW(
        std::wstring(filename, filename + strlen(filename)).c_str());
    int needed = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, NULL, 0, NULL, NULL);
    if (needed <= 0) return std::string("logs/") + filename;
    std::string result(needed - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], needed, NULL, NULL);
    return result;
}

} // namespace clawdesk
