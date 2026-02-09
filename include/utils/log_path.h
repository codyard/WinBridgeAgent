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
#ifndef CLAWDESK_UTILS_LOG_PATH_H
#define CLAWDESK_UTILS_LOG_PATH_H

#include <windows.h>
#include <string>

namespace clawdesk {

// 获取日志目录的宽字符路径（优先 %LOCALAPPDATA%/WinBridgeAgent/logs，
// fallback 到 exe 目录/logs）
std::wstring GetLogDirW();

// 递归创建日志目录（包含父目录 WinBridgeAgent）
void EnsureLogDir();

// 获取指定日志文件的完整宽字符路径
// 例如 GetLogFilePathW(L"audit.log") → "%LOCALAPPDATA%/WinBridgeAgent/logs/audit.log"
std::wstring GetLogFilePathW(const wchar_t* filename);

// 获取指定日志文件的完整窄字符路径（UTF-8）
// 例如 GetLogFilePathA("audit.log") → "%LOCALAPPDATA%/WinBridgeAgent/logs/audit.log"
std::string GetLogFilePathA(const char* filename);

} // namespace clawdesk

#endif // CLAWDESK_UTILS_LOG_PATH_H
