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
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <vector>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <cstring>
#include <exception>
#include <new>
#include <nlohmann/json.hpp>
#include <atomic>
#include "support/config_manager.h"
#include "support/audit_logger.h"
#include "support/dashboard_window.h"
#include "support/license_manager.h"
#include "support/localization_manager.h"
#include "support/settings_window.h"
#include "support/update_checker.h"
#include "support/download_progress_window.h"
#include "policy/policy_guard.h"
#include "services/process_service.h"
#include "services/file_operation_service.h"
#include "services/power_service.h"
#include "services/screenshot_service.h"
#include "services/file_service.h"
#include "services/clipboard_service.h"
#include "services/window_service.h"
#include "services/app_service.h"
#include "services/command_service.h"
#include "services/browser_service.h"
#include "mcp/tool_registry.h"
#include "utils/log_path.h"
#include "mcp_handlers.h"
#include "http_routes.h"
#include "http_server.h"
#include "mcp_sse.h"
#include "tray_app.h"
#include "app_globals.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dbghelp.lib")

using namespace Gdiplus;

// 全局变量
#define WM_TRAYICON (WM_USER + 1)
#define WM_EXIT_COMMAND (WM_USER + 2)
#define WM_UPDATE_CHECK_RESULT (WM_USER + 3)
#define WM_DOWNLOAD_COMPLETE (WM_USER + 4)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ABOUT 1002
#define ID_TRAY_STATUS 1003
#define ID_TRAY_USAGE 1004
#define ID_TRAY_LOGS 1005
#define ID_TRAY_CONFIG 1006
#define ID_TRAY_TOGGLE_LISTEN 1007
#define ID_TRAY_DASHBOARD 1008
#define ID_TRAY_SETTINGS 1009
#define ID_TRAY_CHECK_UPDATE 1010
#define ID_TRAY_UPDATE_SETTINGS 1011
#define CLAWDESK_MAIN_MUTEX_NAME L"WinBridgeAgentServer_SingleInstance"
#define CLAWDESK_DAEMON_MUTEX_NAME L"WinBridgeAgentDaemon_SingleInstance"
#define CLAWDESK_DAEMON_EXIT_EVENT L"WinBridgeAgentDaemon_Exit"

#ifndef CLAWDESK_VERSION
#define CLAWDESK_VERSION "0.3.0"
#endif

NOTIFYICONDATA nid;
HWND g_hwnd = NULL;
std::atomic<bool> g_running(true);
HINSTANCE g_hInstance = NULL;
DWORD g_mainThreadId = 0;
ConfigManager* g_configManager = nullptr;
clawdesk::DashboardWindow* g_dashboard = nullptr;
clawdesk::AuditLogger* g_auditLogger = nullptr;
LicenseManager* g_licenseManager = nullptr;
clawdesk::UpdateChecker* g_updateChecker = nullptr;
std::thread* g_updateCheckThread = nullptr;
std::atomic<bool> g_checkingUpdate(false);
clawdesk::UpdateCheckResult* g_updateCheckResult = nullptr;
PolicyGuard* g_policyGuard = nullptr;

// 定时检查更新相关
std::thread* g_updateTimerThread = nullptr;
std::atomic<bool> g_updateTimerRunning(false);

// HTTP 服务器启动状态
HANDLE g_httpServerStartedEvent = NULL;
std::atomic<bool> g_httpServerStartedOK(false);
LocalizationManager* g_localizationManager = nullptr;
SettingsWindow* g_settingsWindow = nullptr;
ProcessService* g_processService = nullptr;
FileOperationService* g_fileOperationService = nullptr;
PowerService* g_powerService = nullptr;
ScreenshotService* g_screenshotService = nullptr;
FileService* g_fileService = nullptr;
ClipboardService* g_clipboardService = nullptr;
WindowService* g_windowService = nullptr;
AppService* g_appService = nullptr;
CommandService* g_commandService = nullptr;
BrowserService* g_browserService = nullptr;
ULONG_PTR g_gdiplusToken = 0;

namespace {
std::string TrimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }
    if (start == value.size()) {
        return "";
    }
    size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end]))) {
        end--;
    }
    return value.substr(start, end - start + 1);
}

std::string ToLowerStr(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::wstring GetExeDirW() {
    wchar_t path[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        return L"";
    }
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L"";
    }
    return full.substr(0, pos);
}

bool IsDaemonRunning() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, CLAWDESK_DAEMON_MUTEX_NAME);
    if (mutex) {
        CloseHandle(mutex);
        return true;
    }
    return false;
}

} // namespace (end of file-local helpers)

void SignalDaemonExit() {
    HANDLE evt = OpenEventW(EVENT_MODIFY_STATE, FALSE, CLAWDESK_DAEMON_EXIT_EVENT);
    if (!evt) {
        evt = CreateEventW(NULL, TRUE, FALSE, CLAWDESK_DAEMON_EXIT_EVENT);
    }
    if (evt) {
        SetEvent(evt);
        CloseHandle(evt);
    }
}

void CloseDashboardWindow() {
    if (!g_dashboard) {
        return;
    }
    HWND dash = g_dashboard->getHandle();
    if (!dash) {
        return;
    }
    g_dashboard->setForceClose(true);
    PostMessage(dash, WM_CLOSE, 0, 0);
}

std::wstring GetLogDirW() {
    return clawdesk::GetLogDirW();
}

void EnsureLogDir() {
    clawdesk::EnsureLogDir();
}

std::wstring FormatTimestampW() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buffer[64];
    swprintf(buffer, 64, L"%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

void AppendCrashLog(const std::wstring& line) {
    EnsureLogDir();
    std::wstring path = GetLogDirW() + L"\\crash.log";
    
    // 转换为 UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return;
    
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, &utf8[0], len, NULL, NULL);
    
    // 使用标准文件流
    FILE* f = _wfopen(path.c_str(), L"a");
    if (f) {
        fprintf(f, "%s\n", utf8.c_str());
        fclose(f);
    }
}

std::string RedactAuthorizationHeader(const std::string& request) {
    // Avoid leaking bearer token in logs.
    std::string out = request;
    const std::string needle = "Authorization:";
    size_t pos = 0;
    while ((pos = out.find(needle, pos)) != std::string::npos) {
        size_t lineEnd = out.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            lineEnd = out.size();
        }
        out.replace(pos, lineEnd - pos, "Authorization: <redacted>");
        pos += needle.size();
    }
    return out;
}

void AppendExceptionLogA(const std::string& line) {
    EnsureLogDir();
    std::wstring path = GetLogDirW() + L"\\exceptions.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    std::string out = line + "\r\n";
    WriteFile(file, out.c_str(), static_cast<DWORD>(out.size()), &written, NULL);
    CloseHandle(file);
}

void AppendHttpServerLogA(const std::string& line) {
    EnsureLogDir();
    std::wstring path = GetLogDirW() + L"\\http_server.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    std::string out = line + "\r\n";
    WriteFile(file, out.c_str(), static_cast<DWORD>(out.size()), &written, NULL);
    CloseHandle(file);
}

void WriteCrashLog(EXCEPTION_POINTERS* info) {
    std::wstring ts = FormatTimestampW();
    AppendCrashLog(L"==== Crash Report ====");
    AppendCrashLog(L"Time: " + ts);
    
    // 安全地转换版本号
    std::string versionStr(CLAWDESK_VERSION);
    std::wstring versionW(versionStr.begin(), versionStr.end());
    AppendCrashLog(L"Version: " + versionW);
    
    AppendCrashLog(L"ThreadId: " + std::to_wstring(GetCurrentThreadId()));
    if (info && info->ExceptionRecord) {
        wchar_t codeBuf[32];
        swprintf(codeBuf, 32, L"0x%08X", info->ExceptionRecord->ExceptionCode);
        AppendCrashLog(std::wstring(L"ExceptionCode: ") + codeBuf);
        wchar_t addrBuf[32];
        swprintf(addrBuf, 32, L"%p", info->ExceptionRecord->ExceptionAddress);
        AppendCrashLog(std::wstring(L"ExceptionAddress: ") + addrBuf);
    } else {
        AppendCrashLog(L"Exception: <no exception record>");
    }
}

void WriteMiniDump(EXCEPTION_POINTERS* info) {
    EnsureLogDir();
    std::wstring dumpPath = GetLogDirW() + L"\\crash_" + FormatTimestampW() + L".dmp";
    HANDLE hFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        AppendCrashLog(L"MiniDump: failed to create file");
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION exInfo{};
    exInfo.ThreadId = GetCurrentThreadId();
    exInfo.ExceptionPointers = info;
    exInfo.ClientPointers = FALSE;

    MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithFullMemory |
        MiniDumpWithFullMemoryInfo |
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithFullAuxiliaryState |
        MiniDumpIgnoreInaccessibleMemory);

    BOOL ok = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        dumpType,
        info ? &exInfo : nullptr,
        nullptr,
        nullptr);

    CloseHandle(hFile);
    if (ok) {
        AppendCrashLog(L"MiniDump: saved to " + dumpPath);
    } else {
        AppendCrashLog(L"MiniDump: failed to write");
    }
}

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info) {
    WriteCrashLog(info);
    WriteMiniDump(info);
    return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(CrashHandler);

    std::set_terminate([]() {
        AppendCrashLog(L"==== std::terminate ====");
        WriteMiniDump(nullptr);
        abort();
    });

    std::set_new_handler([]() {
        AppendCrashLog(L"==== std::bad_alloc (new_handler) ====");
        WriteMiniDump(nullptr);
        throw std::bad_alloc();
    });
}

void StartDaemonIfNeeded(ConfigManager* configManager) {
    if (!configManager || !configManager->isDaemonEnabled()) {
        return;
    }
    if (IsDaemonRunning()) {
        return;
    }
    std::wstring dir = GetExeDirW();
    if (dir.empty()) {
        return;
    }
    std::wstring daemonPath = dir + L"\\WinBridgeAgentDaemon.exe";
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (CreateProcessW(daemonPath.c_str(), NULL, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

namespace {
bool TryParseKey(const std::string& token, WORD* vk) {
    if (!vk) {
        return false;
    }
    std::string t = ToLowerStr(TrimString(token));
    if (t.empty()) {
        return false;
    }
    if (t == "ctrl" || t == "control") {
        *vk = VK_CONTROL;
        return true;
    }
    if (t == "shift") {
        *vk = VK_SHIFT;
        return true;
    }
    if (t == "alt") {
        *vk = VK_MENU;
        return true;
    }
    if (t == "win" || t == "windows" || t == "cmd") {
        *vk = VK_LWIN;
        return true;
    }
    if (t == "tab") {
        *vk = VK_TAB;
        return true;
    }
    if (t == "enter" || t == "return") {
        *vk = VK_RETURN;
        return true;
    }
    if (t == "esc" || t == "escape") {
        *vk = VK_ESCAPE;
        return true;
    }
    if (t == "space") {
        *vk = VK_SPACE;
        return true;
    }
    if (t == "backspace") {
        *vk = VK_BACK;
        return true;
    }
    if (t == "delete" || t == "del") {
        *vk = VK_DELETE;
        return true;
    }
    if (t == "insert" || t == "ins") {
        *vk = VK_INSERT;
        return true;
    }
    if (t == "home") {
        *vk = VK_HOME;
        return true;
    }
    if (t == "end") {
        *vk = VK_END;
        return true;
    }
    if (t == "pgup" || t == "pageup") {
        *vk = VK_PRIOR;
        return true;
    }
    if (t == "pgdn" || t == "pagedown") {
        *vk = VK_NEXT;
        return true;
    }
    if (t == "up") {
        *vk = VK_UP;
        return true;
    }
    if (t == "down") {
        *vk = VK_DOWN;
        return true;
    }
    if (t == "left") {
        *vk = VK_LEFT;
        return true;
    }
    if (t == "right") {
        *vk = VK_RIGHT;
        return true;
    }
    if (t.size() == 1) {
        char c = t[0];
        if (c >= 'a' && c <= 'z') {
            *vk = static_cast<WORD>('A' + (c - 'a'));
            return true;
        }
        if (c >= '0' && c <= '9') {
            *vk = static_cast<WORD>('0' + (c - '0'));
            return true;
        }
    }
    if (t.size() >= 2 && t[0] == 'f') {
        int fnum = atoi(t.substr(1).c_str());
        if (fnum >= 1 && fnum <= 24) {
            *vk = static_cast<WORD>(VK_F1 + (fnum - 1));
            return true;
        }
    }
    return false;
}
} // namespace

static std::wstring GetExecutableDirW() {
    wchar_t path[MAX_PATH] = {0};
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (len == 0) {
        return L"";
    }
    std::wstring full(path, len);
    size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L"";
    }
    full.resize(pos);
    return full;
}
// 以下前向声明仅用于本文件内定义顺序依赖（全局声明见 app_globals.h）
static std::string ToLower(const std::string& s);

std::wstring Localize(const char* key, const wchar_t* fallback) {
    if (g_localizationManager) {
        return g_localizationManager->getString(key);
    }
    return fallback ? fallback : L"";
}

// 窗口枚举结构
struct LegacyWindowInfo {
    HWND hwnd;
    std::string title;
    std::string className;
    bool isVisible;
    bool isMinimized;
    bool isMaximized;
    DWORD processId;
    RECT rect;
};

std::vector<LegacyWindowInfo> g_windows;

// URL 解码函数
std::string UrlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            char hex[3] = {str[i+1], str[i+2], 0};
            int value = strtol(hex, nullptr, 16);
            result += (char)value;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string GetHeaderValue(const std::string& request, const std::string& headerNameLower) {
    size_t pos = 0;
    while (true) {
        size_t lineEnd = request.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            break;
        }
        if (lineEnd == pos) {
            break; // end of headers
        }

        std::string line = request.substr(pos, lineEnd - pos);
        std::string lower = ToLower(line);
        std::string prefix = headerNameLower + ":";
        if (lower.rfind(prefix, 0) == 0) {
            std::string value = line.substr(prefix.size());
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos) {
                value = value.substr(start);
            } else {
                value.clear();
            }
            return value;
        }

        pos = lineEnd + 2;
    }

    return "";
}

std::string BuildUrlFromRequest(const std::string& request, const std::string& path) {
    std::string host = GetHeaderValue(request, "host");
    if (host.empty()) {
        return path;
    }
    return "http://" + host + path;
}

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), NULL, 0);
    if (sizeNeeded <= 0) {
        return L"";
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), &result[0], sizeNeeded);
    return result;
}

static bool SaveBitmapToPng(Bitmap* bitmap, std::string& outPath) {
    if (!bitmap) {
        return false;
    }

    CreateDirectory("clipboard_images", NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[128];
    snprintf(filename, sizeof(filename), "clipboard_images/clipboard_%04d%02d%02d_%02d%02d%02d.png",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    outPath = filename;

    CLSID clsid;
    if (GetEncoderClsid(L"image/png", &clsid) == -1) {
        return false;
    }

    std::wstring widePath = ToWide(outPath);
    Status status = bitmap->Save(widePath.c_str(), &clsid, NULL);
    return status == Ok;
}

bool SaveClipboardImageFromOpenClipboard(std::string& outPath) {
    if (IsClipboardFormatAvailable(CF_DIB)) {
        HGLOBAL hDib = GetClipboardData(CF_DIB);
        if (hDib) {
            void* pDib = GlobalLock(hDib);
            if (pDib) {
                BITMAPINFO* info = reinterpret_cast<BITMAPINFO*>(pDib);
                BYTE* bits = reinterpret_cast<BYTE*>(pDib) + info->bmiHeader.biSize;
                if (info->bmiHeader.biClrUsed != 0) {
                    bits += info->bmiHeader.biClrUsed * sizeof(RGBQUAD);
                } else if (info->bmiHeader.biBitCount <= 8) {
                    bits += (1 << info->bmiHeader.biBitCount) * sizeof(RGBQUAD);
                }

                Bitmap* bitmap = Bitmap::FromBITMAPINFO(info, bits);
                bool saved = SaveBitmapToPng(bitmap, outPath);
                delete bitmap;
                GlobalUnlock(hDib);
                if (saved) {
                    return true;
                }
            }
        }
    }

    if (IsClipboardFormatAvailable(CF_BITMAP)) {
        HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
        if (hBitmap) {
            Bitmap* bitmap = Bitmap::FromHBITMAP(hBitmap, NULL);
            bool saved = SaveBitmapToPng(bitmap, outPath);
            delete bitmap;
            return saved;
        }
    }

    return false;
}

bool SaveClipboardFilesFromOpenClipboard(std::vector<std::string>& outPaths, std::vector<std::string>& outNames) {
    if (!IsClipboardFormatAvailable(CF_HDROP)) {
        return false;
    }

    HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
    if (!hDrop) {
        return false;
    }

    UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
    if (count == 0) {
        return false;
    }

    CreateDirectory("clipboard_files", NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char prefixBuf[64];
    snprintf(prefixBuf, sizeof(prefixBuf), "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    const std::string prefix(prefixBuf);

    auto sanitizeNameComponent = [](const std::string& input) -> std::string {
        std::string out;
        out.reserve(input.size());
        for (unsigned char c : input) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.') {
                out.push_back(static_cast<char>(c));
            } else {
                out.push_back('_');
            }
        }
        if (out.empty()) {
            out = "file";
        }
        return out;
    };

    auto buildSafeDestName = [&](unsigned index, const std::string& baseName) -> std::string {
        // Keep it URL-safe because we return it as part of /clipboard/file/<name>.
        std::string safeBase = sanitizeNameComponent(baseName);
        std::string idx = std::to_string(index);
        std::string head = prefix + "_" + idx + "_";

        // Be conservative to avoid MAX_PATH issues when combined with "clipboard_files/".
        constexpr size_t kMaxNameLen = 180;
        std::string candidate = head + safeBase;
        if (candidate.size() <= kMaxNameLen) {
            return candidate;
        }

        // Truncate while preserving a short extension if present.
        std::string ext;
        size_t dot = safeBase.find_last_of('.');
        if (dot != std::string::npos && dot > 0) {
            std::string maybeExt = safeBase.substr(dot);
            if (maybeExt.size() <= 16) {
                ext = maybeExt;
                safeBase = safeBase.substr(0, dot);
            }
        }

        size_t budget = kMaxNameLen;
        if (head.size() + ext.size() < budget) {
            budget -= (head.size() + ext.size());
        } else {
            // Extremely unlikely; fall back to a minimal name.
            return head + "file" + ext;
        }

        if (budget < 8) {
            budget = 8;
        }
        if (safeBase.size() > budget) {
            safeBase.resize(budget);
        }
        return head + safeBase + ext;
    };

    for (UINT i = 0; i < count; ++i) {
        char path[MAX_PATH];
        if (DragQueryFile(hDrop, i, path, MAX_PATH) == 0) {
            continue;
        }

        DWORD attrs = GetFileAttributes(path);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            continue; // skip directories
        }

        char* base = PathFindFileName(path);
        if (!base || strlen(base) == 0) {
            continue;
        }

        std::string destName = buildSafeDestName(i, base);
        std::string destPath = std::string("clipboard_files/") + destName;

        if (CopyFileA(path, destPath.c_str(), FALSE)) {
            outPaths.push_back(destPath);
            outNames.push_back(destName);
        }
    }

    return !outPaths.empty();
}

bool SaveScreenshotToFile(const std::string& format, std::string& outPath, int& width, int& height) {
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOldBitmap);

    Bitmap* bitmap = Bitmap::FromHBITMAP(hBitmap, NULL);

    CreateDirectory("screenshots", NULL);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[128];
    const char* ext = (format == "jpg" || format == "jpeg") ? "jpg" : "png";
    snprintf(filename, sizeof(filename), "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d.%s",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ext);
    outPath = filename;

    CLSID clsid;
    const WCHAR* mimeType = L"image/png";
    if (format == "jpg" || format == "jpeg") {
        mimeType = L"image/jpeg";
    }

    bool ok = false;
    if (GetEncoderClsid(mimeType, &clsid) != -1) {
        std::wstring widePath = ToWide(outPath);
        Status status = bitmap->Save(widePath.c_str(), &clsid, NULL);
        ok = (status == Ok);
    }

    delete bitmap;
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return ok;
}

static std::string ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// Open-source edition: auth check disabled, always allow.
bool IsAuthorizedRequest(const std::string& /*request*/) {
    return true;
}

std::string MakeUnauthorizedResponse() {
    const char* body = "{\"error\":\"unauthorized\"}";
    char header[256];
    int contentLength = static_cast<int>(strlen(body));
    snprintf(header, sizeof(header),
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        contentLength
    );
    return std::string(header) + body;
}

// 获取 CLSID 用于图像编码器
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    
    GetImageEncoders(num, size, pImageCodecInfo);
    
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    
    free(pImageCodecInfo);
    return -1;
}

// Base64 编码函数
std::string Base64Encode(const unsigned char* data, size_t length) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; j < i + 1; j++)
            result += base64_chars[char_array_4[j]];
        
        while (i++ < 3)
            result += '=';
    }
    
    return result;
}

// 捕获屏幕截图并返回 Base64 编码的图像数据
std::string CaptureScreenshot(const std::string& format) {
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // 创建设备上下文
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // 复制屏幕内容到位图
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOldBitmap);
    
    // 使用 GDI+ 将位图转换为图像格式
    Bitmap* bitmap = Bitmap::FromHBITMAP(hBitmap, NULL);
    
    // 创建内存流
    IStream* stream = NULL;
    HRESULT hrStream = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(hrStream) || !stream) {
        delete bitmap;
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return "";
    }
    
    // 获取编码器 CLSID
    CLSID clsid;
    const WCHAR* mimeType = L"image/png";
    if (format == "jpg" || format == "jpeg") {
        mimeType = L"image/jpeg";
    }
    
    if (GetEncoderClsid(mimeType, &clsid) == -1) {
        delete bitmap;
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return "";
    }
    
    // 保存到流
    bitmap->Save(stream, &clsid, NULL);
    
    // 获取流大小
    STATSTG statstg;
    HRESULT hrStat = stream->Stat(&statstg, STATFLAG_DEFAULT);
    if (FAILED(hrStat)) {
        stream->Release();
        delete bitmap;
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return "";
    }
    ULONG size = statstg.cbSize.LowPart;
    
    // 读取流数据
    HGLOBAL hGlobal;
    HRESULT hrGlobal = GetHGlobalFromStream(stream, &hGlobal);
    if (FAILED(hrGlobal) || !hGlobal) {
        stream->Release();
        delete bitmap;
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return "";
    }
    void* pData = GlobalLock(hGlobal);
    if (!pData) {
        stream->Release();
        delete bitmap;
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return "";
    }
    
    // Base64 编码
    std::string base64Data = Base64Encode((const unsigned char*)pData, size);
    
    // 清理
    GlobalUnlock(hGlobal);
    stream->Release();
    delete bitmap;
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    return base64Data;
}

// 窗口枚举回调函数
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    // 跳过不可见窗口（可选）
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    
    // 获取窗口标题
    char title[256] = {0};
    GetWindowText(hwnd, title, sizeof(title));
    
    // 跳过没有标题的窗口
    if (strlen(title) == 0) {
        return TRUE;
    }
    
    // 获取窗口类名
    char className[256] = {0};
    GetClassName(hwnd, className, sizeof(className));
    
    // 获取进程 ID
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    
    // 获取窗口位置和大小
    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    // 检查窗口状态
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &placement);
    
    LegacyWindowInfo info;
    info.hwnd = hwnd;
    info.title = title;
    info.className = className;
    info.isVisible = IsWindowVisible(hwnd);
    info.isMinimized = (placement.showCmd == SW_SHOWMINIMIZED);
    info.isMaximized = (placement.showCmd == SW_SHOWMAXIMIZED);
    info.processId = processId;
    info.rect = rect;
    
    g_windows.push_back(info);
    
    return TRUE;
}

// 获取窗口列表
std::string GetWindowList() {
    g_windows.clear();
    EnumWindows(EnumWindowsProc, 0);

    // Avoid fixed-size buffers and manual escaping (can overflow and crash).
    nlohmann::json out = nlohmann::json::array();
    for (const auto& win : g_windows) {
        out.push_back({
            {"hwnd", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(win.hwnd))},
            {"title", win.title},
            {"class", win.className},
            {"visible", win.isVisible},
            {"minimized", win.isMinimized},
            {"maximized", win.isMaximized},
            {"process_id", win.processId},
            {"x", win.rect.left},
            {"y", win.rect.top},
            {"width", win.rect.right - win.rect.left},
            {"height", win.rect.bottom - win.rect.top}
        });
    }
    return out.dump();
}

// 获取进程列表
std::string GetProcessList() {
    nlohmann::json out = nlohmann::json::array();
    
    // 创建进程快照
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return "[]";
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            std::string name = pe32.szExeFile;
            
            // 尝试打开进程获取更多信息
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            
            std::string exePath = "";
            DWORD memoryUsage = 0;
            
            if (hProcess) {
                // 获取进程路径
                char path[MAX_PATH] = {0};
                if (GetModuleFileNameEx(hProcess, NULL, path, MAX_PATH)) {
                    exePath = path;
                }
                
                // 获取内存使用
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    memoryUsage = pmc.WorkingSetSize / 1024; // KB
                }
                
                CloseHandle(hProcess);
            }

            out.push_back({
                {"pid", pe32.th32ProcessID},
                {"name", name},
                {"path", exePath},
                {"threads", pe32.cntThreads},
                {"parent_pid", pe32.th32ParentProcessID},
                {"memory_kb", memoryUsage}
            });
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return out.dump();
}


// 执行命令并捕获输出（带可配置超时）
std::string ExecuteCommand(const std::string& command) {
    std::string trimmed = command;
    trimmed.erase(trimmed.begin(),
                  std::find_if(trimmed.begin(), trimmed.end(),
                               [](unsigned char c) { return !std::isspace(c); }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(),
                               [](unsigned char c) { return !std::isspace(c); }).base(),
                  trimmed.end());
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.empty() || lower == "/" || lower == "help") {
        return BuildHelpJson();
    }

    // 从配置读取超时（秒）
    int timeoutSec = g_configManager ? g_configManager->getCommandTimeoutSeconds() : 30;
    DWORD timeoutMs = static_cast<DWORD>(timeoutSec) * 1000;

    std::string output;
    
    // 创建管道
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return "{\"error\":\"Failed to create pipe\"}";
    }
    
    // 设置读取端不可继承
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    // 创建进程
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // 构建命令行
    std::string cmdLine = "cmd.exe /c " + command;
    char* cmdLineBuffer = new char[cmdLine.length() + 1];
    strncpy(cmdLineBuffer, cmdLine.c_str(), cmdLine.length() + 1);
    
    BOOL success = CreateProcess(
        NULL,
        cmdLineBuffer,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    
    delete[] cmdLineBuffer;
    
    if (!success) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return "{\"error\":\"Failed to create process\"}";
    }
    
    // 关闭写入端（子进程退出后读取端会收到 EOF）
    CloseHandle(hWritePipe);
    
    // 带超时的非阻塞读取：用 PeekNamedPipe 轮询，避免 ReadFile 无限阻塞
    DWORD startTick = GetTickCount();
    bool timedOut = false;
    char buffer[4096];

    while (true) {
        // 检查超时
        if ((GetTickCount() - startTick) > timeoutMs) {
            timedOut = true;
            break;
        }

        // 检查管道中是否有数据
        DWORD bytesAvail = 0;
        if (!PeekNamedPipe(hReadPipe, NULL, 0, NULL, &bytesAvail, NULL)) {
            break; // 管道已关闭（进程退出）
        }

        if (bytesAvail > 0) {
            DWORD bytesRead = 0;
            DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
            if (ReadFile(hReadPipe, buffer, toRead, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            }
        } else {
            // 没数据，检查进程是否已退出
            DWORD waitResult = WaitForSingleObject(pi.hProcess, 100);
            if (waitResult == WAIT_OBJECT_0) {
                // 进程已退出，读完剩余数据
                DWORD bytesRead = 0;
                while (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
                    DWORD toRead2 = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
                    if (ReadFile(hReadPipe, buffer, toRead2, &bytesRead, NULL) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        output += buffer;
                    } else {
                        break;
                    }
                }
                break;
            }
            // 进程还在跑但没输出，短暂等待避免 CPU 空转
        }
    }

    // 超时则强杀进程
    if (timedOut) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 2000);
        AppendHttpServerLogA("[ExecuteCommand] Process killed after " + std::to_string(timeoutSec) + "s timeout");
    }
    
    // 获取退出代码
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // 清理
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    nlohmann::json payload;
    payload["success"] = !timedOut;
    payload["exit_code"] = exitCode;
    payload["output"] = output;
    if (timedOut) {
        payload["error"] = "Command timed out after " + std::to_string(timeoutSec) + " seconds";
    }
    return payload.dump();
}

// HTTP 服务器线程
DWORD WINAPI HttpServerThread(LPVOID lpParam);
HANDLE g_serverThread = NULL;
SOCKET g_serverSocket = INVALID_SOCKET;
DWORD  g_startTickCount = 0;

// 解析 HTTP 请求行，提取 method 和 path（不含 query string）

std::string BuildHelpJson() {
    nlohmann::json apiList;
    apiList["name"] = "WinBridgeAgent Server";
    apiList["version"] = CLAWDESK_VERSION;
    apiList["status"] = "running";
    apiList["endpoints"] = nlohmann::json::array({
        {{"path", "/"}, {"method", "GET"}, {"description", "API endpoint list"}},
        {{"path", "/help"}, {"method", "GET"}, {"description", "API endpoint list"}},
        {{"path", "/health"}, {"method", "GET"}, {"description", "Health check"}},
        {{"path", "/status"}, {"method", "GET"}, {"description", "Server status and info"}},
        {{"path", "/sts"}, {"method", "GET"}, {"description", "Alias for /status"}},
        {{"path", "/disks"}, {"method", "GET"}, {"description", "List all disk drives with details"}},
        {{"path", "/list?path=<path>"}, {"method", "GET"}, {"description", "List directory contents"}},
        {{"path", "/search?path=<path>&query=<text>&case=<i|sensitive>&max=<n>"}, {"method", "GET"}, {"description", "Search text in file"}},
        {{"path", "/read?path=<path>&start=<n>&lines=<n>&tail=<n>&count=<bool>"}, {"method", "GET"}, {"description", "Read file content with line range support"}},
        {{"path", "/clipboard"}, {"method", "GET"}, {"description", "Get clipboard content"}},
        {{"path", "/clipboard"}, {"method", "PUT"}, {"description", "Set clipboard content (JSON body: {\"content\":\"text\"})"}},
        {{"path", "/clipboard/image/<name>"}, {"method", "GET"}, {"description", "Get clipboard image file"}},
        {{"path", "/clipboard/file/<name>"}, {"method", "GET"}, {"description", "Get clipboard file"}},
        {{"path", "/screenshot?format=<png|jpg>"}, {"method", "GET"}, {"description", "Capture screenshot (returns URL)"}},
        {{"path", "/screenshot/file/<name>"}, {"method", "GET"}, {"description", "Get screenshot file"}},
        {{"path", "/windows"}, {"method", "GET"}, {"description", "List all open windows"}},
        {{"path", "/processes"}, {"method", "GET"}, {"description", "List all running processes"}},
        {{"path", "/execute"}, {"method", "POST"}, {"description", "Execute command (JSON body: {\"command\":\"cmd\"})"}},
        {{"path", "/mcp/initialize"}, {"method", "POST"}, {"description", "MCP: Initialize connection"}},
        {{"path", "/mcp/tools/list"}, {"method", "POST"}, {"description", "MCP: List available tools"}},
        {{"path", "/mcp/tools/call"}, {"method", "POST"}, {"description", "MCP: Call a tool"}},
        {{"path", "/mcp"}, {"method", "POST"}, {"description", "MCP Streamable HTTP (JSON-RPC 2.0)"}},
        {{"path", "/sse"}, {"method", "GET"}, {"description", "MCP SSE transport (Server-Sent Events)"}},
        {{"path", "/messages?sessionId=<id>"}, {"method", "POST"}, {"description", "MCP SSE: Send JSON-RPC message to SSE session"}},
        {{"path", "/reload"}, {"method", "GET"}, {"description", "Hot-reload config.json without restart"}},
        {{"path", "/exit"}, {"method", "GET"}, {"description", "Shutdown server"}}
    });

    auto tools = ToolRegistry::getInstance().getAllTools();
    nlohmann::json toolList = nlohmann::json::array();
    for (const auto& tool : tools) {
        toolList.push_back({
            {"name", tool.name},
            {"description", tool.description}
        });
    }
    apiList["mcp_tools"] = toolList;
    return apiList.dump();
}

bool IsModifierKey(WORD vk) {
    return vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT ||
           vk == VK_LWIN || vk == VK_RWIN;
}

bool SendHotkey(const std::string& hotkey) {
    std::vector<WORD> modifiers;
    std::vector<WORD> keys;
    size_t start = 0;
    while (start < hotkey.size()) {
        size_t plus = hotkey.find('+', start);
        std::string token = (plus == std::string::npos)
            ? hotkey.substr(start)
            : hotkey.substr(start, plus - start);
        start = (plus == std::string::npos) ? hotkey.size() : plus + 1;
        WORD vk = 0;
        if (!TryParseKey(token, &vk)) {
            return false;
        }
        if (IsModifierKey(vk)) {
            modifiers.push_back(vk);
        } else {
            keys.push_back(vk);
        }
    }
    if (keys.empty()) {
        return false;
    }

    std::vector<INPUT> inputs;
    inputs.reserve((modifiers.size() + keys.size()) * 2);
    for (WORD vk : modifiers) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        inputs.push_back(input);
    }
    for (WORD vk : keys) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        inputs.push_back(input);
    }
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = *it;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(input);
    }
    for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = *it;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(input);
    }

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent == inputs.size();
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_mainThreadId = GetCurrentThreadId();
    g_startTickCount = GetTickCount();
    InstallCrashHandler();
    HANDLE instanceMutex = CreateMutexW(NULL, TRUE, CLAWDESK_MAIN_MUTEX_NAME);
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowA("WinBridgeAgentServerClass", NULL);
        if (existing) {
            PostMessage(existing, WM_EXIT_COMMAND, 0, 0);
        }
        DWORD waitResult = WaitForSingleObject(instanceMutex, 8000);
        if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
            MessageBox(NULL,
                       "WinBridgeAgent Server is still running. Please close it first.",
                       "WinBridgeAgent Server",
                       MB_OK | MB_ICONINFORMATION);
            CloseHandle(instanceMutex);
            return 0;
        }
    }
    auto closeInstanceMutex = [&]() {
        if (instanceMutex) {
            CloseHandle(instanceMutex);
            instanceMutex = NULL;
        }
    };

    g_hInstance = hInstance;
    std::wstring exeDir = GetExecutableDirW();
    if (!exeDir.empty()) {
        SetCurrentDirectoryW(exeDir.c_str());
    }
    // 初始化 GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    
    // 创建日志目录（使用统一的日志目录路径）
    EnsureLogDir();
    
    // 初始化配置管理器（堆分配）
    g_configManager = new ConfigManager("config.json");
    try {
        g_configManager->load();
    } catch (const std::exception& e) {
        MessageBox(NULL, "Failed to load config.json", "Error", MB_OK | MB_ICONERROR);
        GdiplusShutdown(g_gdiplusToken);
        closeInstanceMutex();
        return 1;
    }
    
    // 初始化审计日志（堆分配）
    g_auditLogger = new clawdesk::AuditLogger(clawdesk::GetLogFilePathA("audit.log"));

    // 初始化许可证与策略守卫（堆分配）
    g_licenseManager = new LicenseManager(g_configManager, "usage.json");

    // 初始化本地化管理器（堆分配）
    g_localizationManager = new LocalizationManager();
    g_localizationManager->setLanguage(g_configManager->getLanguage());
    StartDaemonIfNeeded(g_configManager);
    
    // 初始化 UpdateChecker（堆分配）
    g_updateChecker = new clawdesk::UpdateChecker(
        g_configManager->getGitHubRepo(),
        CLAWDESK_VERSION
    );
    g_updateChecker->setConfigManager(g_configManager);
    {
        std::ofstream log(clawdesk::GetLogFilePathA("localization.log"), std::ios::app);
        if (log.is_open()) {
            log << "startup: config.language=" << g_configManager->getLanguage()
                << " currentLanguage=" << g_localizationManager->getCurrentLanguage()
                << std::endl;
        }
    }

    // 初始化策略守卫（堆分配）
    g_policyGuard = new PolicyGuard(g_configManager, g_licenseManager);
    
    // 初始化服务（堆分配）
    g_processService = new ProcessService(g_configManager, g_policyGuard);
    g_fileOperationService = new FileOperationService(g_configManager, g_policyGuard);
    
    g_powerService = new PowerService(g_configManager, g_policyGuard);
    g_screenshotService = new ScreenshotService(g_configManager);
    g_fileService = new FileService(g_configManager, g_policyGuard);
    g_clipboardService = new ClipboardService();
    g_windowService = new WindowService();
    g_appService = new AppService(g_configManager, g_policyGuard);
    g_commandService = new CommandService(g_configManager, g_policyGuard);
    g_browserService = new BrowserService();

    // 注册 MCP 工具
    RegisterMcpTools();
    
    // 初始化 Dashboard（使用堆分配）
    g_dashboard = new clawdesk::DashboardWindow();
    g_dashboard->setLocalizationManager(g_localizationManager);
    
    // 不在启动时创建窗口，等用户点击菜单时再创建
    // 使用 AuditLogger 记录启动日志
    clawdesk::AuditLogEntry logEntry;
    logEntry.time = g_auditLogger->getCurrentTimestamp();
    logEntry.tool = "startup";
    logEntry.risk = clawdesk::RiskLevel::Low;
    logEntry.result = "Dashboard initialized";
    logEntry.duration_ms = 0;
    g_auditLogger->logToolCall(logEntry);
    
    // 检查并添加防火墙规则
    std::string listenAddr = g_configManager->getListenAddress();
    logEntry.result = "Listen address: " + listenAddr;
    g_auditLogger->logToolCall(logEntry);
    if (listenAddr == "0.0.0.0") {
        g_dashboard->logProcessing("firewall", "Checking firewall rules...");
        
        // 只有在监听所有接口时才需要防火墙规则
        if (!CheckFirewallRule()) {
            g_dashboard->logProcessing("firewall", "Firewall rule not found, requesting user permission...");
            
            int result = MessageBox(NULL,
                "WinBridgeAgent Server needs to add a Windows Firewall rule to allow network access.\n\n"
                "This requires administrator privileges.\n\n"
                "Click OK to add the firewall rule, or Cancel to continue without it.",
                "Firewall Configuration",
                MB_OKCANCEL | MB_ICONQUESTION);
            
            if (result == IDOK) {
                g_dashboard->logProcessing("firewall", "Adding firewall rule...");
                
                if (AddFirewallRule()) {
                    g_dashboard->logSuccess("firewall", "Firewall rule added successfully");
                    MessageBox(NULL,
                        "Firewall rule added successfully!",
                        "Success",
                        MB_OK | MB_ICONINFORMATION);
                } else {
                    g_dashboard->logError("firewall", "Failed to add firewall rule");
                    MessageBox(NULL,
                        "Failed to add firewall rule. You may need to add it manually:\n\n"
                        "1. Open Windows Defender Firewall\n"
                        "2. Click 'Advanced settings'\n"
                        "3. Click 'Inbound Rules' -> 'New Rule'\n"
                        "4. Select 'Program' and browse to WinBridgeAgent.exe\n"
                        "5. Allow the connection",
                        "Warning",
                        MB_OK | MB_ICONWARNING);
                }
            } else {
                g_dashboard->logProcessing("firewall", "User cancelled firewall configuration");
            }
        } else {
            g_dashboard->logSuccess("firewall", "Firewall rule already exists");
        }
    }
    
    // 记录启动日志
    clawdesk::AuditLogEntry startEntry;
    startEntry.time = g_auditLogger->getCurrentTimestamp();
    startEntry.tool = "server_start";
    startEntry.risk = clawdesk::RiskLevel::Low;
    startEntry.result = "ok";
    startEntry.duration_ms = 0;
    startEntry.error = "";
    g_auditLogger->logToolCall(startEntry);
    
    // 注册窗口类
    logEntry.result = "Registering window class";
    g_auditLogger->logToolCall(logEntry);
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "WinBridgeAgentServerClass";
    
    if (!RegisterClassEx(&wc)) {
        logEntry.result = "ERROR: Failed to register window class";
        g_auditLogger->logToolCall(logEntry);
        MessageBox(NULL, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
        closeInstanceMutex();
        return 1;
    }
    logEntry.result = "Window class registered";
    g_auditLogger->logToolCall(logEntry);
    
    // 创建隐藏窗口（用于消息处理）
    logEntry.result = "Creating main window";
    g_auditLogger->logToolCall(logEntry);
    
    g_hwnd = CreateWindowEx(
        0,
        "WinBridgeAgentServerClass",
        "WinBridgeAgent Server",
        0,
        0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hwnd) {
        logEntry.result = "ERROR: Failed to create window";
        g_auditLogger->logToolCall(logEntry);
        MessageBox(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        closeInstanceMutex();
        return 1;
    }
    logEntry.result = "Main window created";
    g_auditLogger->logToolCall(logEntry);
    
    // 创建托盘图标
    logEntry.result = "Creating tray icon";
    g_auditLogger->logToolCall(logEntry);
    
    if (!CreateTrayIcon(g_hwnd)) {
        logEntry.result = "ERROR: Failed to create tray icon";
        g_auditLogger->logToolCall(logEntry);
        MessageBox(NULL, "Failed to create tray icon", "Error", MB_OK | MB_ICONERROR);
        DestroyWindow(g_hwnd);
        closeInstanceMutex();
        return 1;
    }
    logEntry.result = "Tray icon created";
    g_auditLogger->logToolCall(logEntry);
    
    // 启动 HTTP 服务器线程
    logEntry.result = "Starting HTTP server thread";
    g_auditLogger->logToolCall(logEntry);
    
    // 创建事件用于等待 HTTP 服务器启动结果
    g_httpServerStartedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_httpServerStartedOK.store(false);
    
    g_serverThread = CreateThread(NULL, 0, HttpServerThread, NULL, 0, NULL);
    if (g_serverThread == NULL) {
        logEntry.result = "ERROR: Failed to start HTTP server thread";
        g_auditLogger->logToolCall(logEntry);
        MessageBox(NULL, "Failed to start HTTP server", "Error", MB_OK | MB_ICONERROR);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        DestroyWindow(g_hwnd);
        if (g_httpServerStartedEvent) {
            CloseHandle(g_httpServerStartedEvent);
            g_httpServerStartedEvent = NULL;
        }
        closeInstanceMutex();
        return 1;
    }
    
    // 等待 HTTP 服务器启动结果（最多 5 秒）
    DWORD waitResult = WaitForSingleObject(g_httpServerStartedEvent, 5000);
    if (g_httpServerStartedEvent) {
        CloseHandle(g_httpServerStartedEvent);
        g_httpServerStartedEvent = NULL;
    }
    
    if (waitResult == WAIT_TIMEOUT || !g_httpServerStartedOK.load()) {
        logEntry.result = "ERROR: HTTP server failed to start (bind/listen failed or timeout)";
        g_auditLogger->logToolCall(logEntry);
        
        int port = g_configManager ? g_configManager->getServerPort() : 35182;
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), 
            "HTTP server failed to start on port %d.\n\n"
            "Possible causes:\n"
            "- Port is already in use\n"
            "- Firewall blocking\n\n"
            "Check logs for details.", port);
        MessageBox(NULL, errMsg, "Error", MB_OK | MB_ICONERROR);
        
        g_running.store(false);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        DestroyWindow(g_hwnd);
        closeInstanceMutex();
        return 1;
    }
    
    logEntry.result = "HTTP server started successfully";
    g_auditLogger->logToolCall(logEntry);
    
    // 显示启动通知
    int port = g_configManager ? g_configManager->getServerPort() : 35182;
    nid.uFlags = NIF_INFO;
    snprintf(nid.szInfoTitle, sizeof(nid.szInfoTitle), "WinBridgeAgent Server");
    snprintf(nid.szInfo, sizeof(nid.szInfo), "Server started on port %d", port);
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
    
    logEntry.result = "Tray notification sent";
    g_auditLogger->logToolCall(logEntry);
    
    // 启动时检查更新（如果配置启用）
#ifdef CLAWDESK_OPENSSL_ENABLED
    if (g_configManager->isAutoUpdateEnabled()) {
        logEntry.result = "Scheduling startup update check";
        g_auditLogger->logToolCall(logEntry);
        
        // 延迟 2 秒后检查更新，避免阻塞启动
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (g_updateChecker && g_hwnd && !g_checkingUpdate.load() && g_configManager) {
                g_checkingUpdate.store(true);
                
                // 获取更新频道（使用全局指针）
                bool isBeta = (g_configManager->getUpdateChannel() == "beta");
                
                g_updateChecker->checkForUpdatesAsync([](const clawdesk::UpdateCheckResult& result) {
                    // 分配结果副本（UI 线程负责释放）
                    clawdesk::UpdateCheckResult* resultCopy = new clawdesk::UpdateCheckResult(result);
                    
                    // 发送消息到 UI 线程
                    if (g_hwnd) {
                        PostMessage(g_hwnd, WM_UPDATE_CHECK_RESULT, 0, reinterpret_cast<LPARAM>(resultCopy));
                    } else {
                        delete resultCopy;
                    }
                }, isBeta);
            }
        }).detach();
    }
#else
    // g_dashboard->logProcessing("auto_update", "OpenSSL disabled, skipping update checks.");
#endif
    
    // 启动定时检查更新线程
#ifdef CLAWDESK_OPENSSL_ENABLED
    if (g_configManager->isAutoUpdateEnabled()) {
        int intervalHours = g_configManager->getUpdateCheckIntervalHours();
        if (intervalHours > 0) {
            logEntry.result = "Starting update timer (interval: " + std::to_string(intervalHours) + " hours)";
            g_auditLogger->logToolCall(logEntry);
            
            g_updateTimerRunning.store(true);
            g_updateTimerThread = new std::thread([intervalHours]() {
                while (g_updateTimerRunning.load()) {
                    // 等待指定的小时数（每分钟检查一次是否需要停止）
                    for (int i = 0; i < intervalHours * 60 && g_updateTimerRunning.load(); ++i) {
                        std::this_thread::sleep_for(std::chrono::minutes(1));
                    }
                    
                    if (!g_updateTimerRunning.load()) break;
                    
                    // 检查上次检查时间，避免重复检查
                    std::string lastCheck = g_configManager ? g_configManager->getLastUpdateCheck() : "";
                    auto now = std::chrono::system_clock::now();
                    
                    // 如果距离上次检查已超过间隔时间，执行检查
                    bool shouldCheck = true;
                    if (!lastCheck.empty()) {
                        // 简单判断：如果有上次检查时间，就检查
                        // 更精确的时间判断可以解析 ISO 8601 格式
                    }
                    
                    if (shouldCheck && g_updateChecker && g_hwnd && !g_checkingUpdate.load() && g_configManager) {
                        g_checkingUpdate.store(true);
                        
                        // 获取更新频道（使用全局指针）
                        bool isBeta = (g_configManager->getUpdateChannel() == "beta");
                        
                        g_updateChecker->checkForUpdatesAsync([](const clawdesk::UpdateCheckResult& result) {
                            clawdesk::UpdateCheckResult* resultCopy = new clawdesk::UpdateCheckResult(result);
                            
                            if (g_hwnd) {
                                PostMessage(g_hwnd, WM_UPDATE_CHECK_RESULT, 0, reinterpret_cast<LPARAM>(resultCopy));
                            } else {
                                delete resultCopy;
                            }
                        }, isBeta);
                    }
                }
                
                if (g_dashboard) g_dashboard->logProcessing("auto_update", "Update timer stopped");
            });
        }
    }
#endif
    
    // 消息循环
    logEntry.result = "Entering message loop";
    g_auditLogger->logToolCall(logEntry);
    
    MSG msg;
    while (g_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 记录停止日志
    clawdesk::AuditLogEntry stopEntry;
    stopEntry.time = g_auditLogger->getCurrentTimestamp();
    stopEntry.tool = "server_stop";
    stopEntry.risk = clawdesk::RiskLevel::Low;
    stopEntry.result = "ok";
    stopEntry.duration_ms = 0;
    stopEntry.error = "";
    g_auditLogger->logToolCall(stopEntry);
    
    // 停止定时检查更新线程
    if (g_updateTimerThread) {
        g_updateTimerRunning.store(false);
        if (g_updateTimerThread->joinable()) {
            g_updateTimerThread->join();
        }
        delete g_updateTimerThread;
        g_updateTimerThread = nullptr;
    }
    
    // 清理
    g_running = false;
    
    // 先关闭所有 SSE session（关闭 socket 让线程尽快退出）
    SseSessionStore::getInstance().shutdownAllSessions();
    
    // 等待服务器线程结束
    if (g_serverThread) {
        WaitForSingleObject(g_serverThread, 5000);
        CloseHandle(g_serverThread);
    }
    
    // 等待 SSE 线程退出（最多 5 秒，每 100ms 检查一次）
    for (int i = 0; i < 50; ++i) {
        if (SseSessionStore::getInstance().sessionCount() == 0) break;
        Sleep(100);
    }
    
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyWindow(g_hwnd);
    
    // 清理 Dashboard（所有后台线程已停止，安全删除）
    if (g_dashboard) {
        delete g_dashboard;
        g_dashboard = nullptr;
    }
    
    // 清理全局服务对象
    if (g_settingsWindow) { delete g_settingsWindow; g_settingsWindow = nullptr; }
    if (g_updateChecker) { delete g_updateChecker; g_updateChecker = nullptr; }
    if (g_updateCheckResult) { delete g_updateCheckResult; g_updateCheckResult = nullptr; }
    if (g_localizationManager) { delete g_localizationManager; g_localizationManager = nullptr; }
    if (g_policyGuard) { delete g_policyGuard; g_policyGuard = nullptr; }
    if (g_licenseManager) { delete g_licenseManager; g_licenseManager = nullptr; }
    if (g_processService) { delete g_processService; g_processService = nullptr; }
    if (g_fileOperationService) { delete g_fileOperationService; g_fileOperationService = nullptr; }
    if (g_powerService) { delete g_powerService; g_powerService = nullptr; }
    if (g_screenshotService) { delete g_screenshotService; g_screenshotService = nullptr; }
    if (g_fileService) { delete g_fileService; g_fileService = nullptr; }
    if (g_clipboardService) { delete g_clipboardService; g_clipboardService = nullptr; }
    if (g_windowService) { delete g_windowService; g_windowService = nullptr; }
    if (g_appService) { delete g_appService; g_appService = nullptr; }
    if (g_commandService) { delete g_commandService; g_commandService = nullptr; }
    if (g_browserService) { delete g_browserService; g_browserService = nullptr; }
    if (g_auditLogger) { delete g_auditLogger; g_auditLogger = nullptr; }
    if (g_configManager) { delete g_configManager; g_configManager = nullptr; }
    
    // 关闭 GDI+
    GdiplusShutdown(g_gdiplusToken);

    closeInstanceMutex();
    return 0;
}
