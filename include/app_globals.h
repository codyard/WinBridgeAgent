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
#ifndef CLAWDESK_APP_GLOBALS_H
#define CLAWDESK_APP_GLOBALS_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <nlohmann/json.hpp>

// 前向声明（避免 include 重量级头文件）
class ConfigManager;
class LicenseManager;
class PolicyGuard;
class LocalizationManager;
class SettingsWindow;
class ProcessService;
class FileOperationService;
class PowerService;
class ScreenshotService;
class FileService;
class ClipboardService;
class WindowService;
class AppService;
class CommandService;
class BrowserService;

namespace clawdesk {
    class DashboardWindow;
    class AuditLogger;
    class UpdateChecker;
    struct UpdateCheckResult;
}

// ── 窗口消息 & 菜单 ID ──────────────────────────────────
#define WM_TRAYICON         (WM_USER + 1)
#define WM_EXIT_COMMAND     (WM_USER + 2)
#define WM_UPDATE_CHECK_RESULT (WM_USER + 3)
#define WM_DOWNLOAD_COMPLETE   (WM_USER + 4)

#define ID_TRAY_EXIT            1001
#define ID_TRAY_ABOUT           1002
#define ID_TRAY_STATUS          1003
#define ID_TRAY_USAGE           1004
#define ID_TRAY_LOGS            1005
#define ID_TRAY_CONFIG          1006
#define ID_TRAY_TOGGLE_LISTEN   1007
#define ID_TRAY_DASHBOARD       1008
#define ID_TRAY_SETTINGS        1009
#define ID_TRAY_CHECK_UPDATE    1010
#define ID_TRAY_UPDATE_SETTINGS 1011

#define CLAWDESK_MAIN_MUTEX_NAME   L"WinBridgeAgentServer_SingleInstance"
#define CLAWDESK_DAEMON_MUTEX_NAME L"WinBridgeAgentDaemon_SingleInstance"
#define CLAWDESK_DAEMON_EXIT_EVENT L"WinBridgeAgentDaemon_Exit"

#ifndef CLAWDESK_VERSION
#define CLAWDESK_VERSION "0.3.0"
#endif

// ── 全局变量 (extern) ───────────────────────────────────
extern NOTIFYICONDATA nid;
extern HWND g_hwnd;
extern std::atomic<bool> g_running;
extern HINSTANCE g_hInstance;
extern DWORD g_mainThreadId;

extern ConfigManager*              g_configManager;
extern clawdesk::DashboardWindow*  g_dashboard;
extern clawdesk::AuditLogger*      g_auditLogger;
extern LicenseManager*             g_licenseManager;
extern clawdesk::UpdateChecker*    g_updateChecker;
extern std::thread*                g_updateCheckThread;
extern std::atomic<bool>           g_checkingUpdate;
extern clawdesk::UpdateCheckResult* g_updateCheckResult;
extern PolicyGuard*                g_policyGuard;

extern std::thread*                g_updateTimerThread;
extern std::atomic<bool>           g_updateTimerRunning;

extern HANDLE                      g_httpServerStartedEvent;
extern std::atomic<bool>           g_httpServerStartedOK;

extern LocalizationManager*        g_localizationManager;
extern SettingsWindow*             g_settingsWindow;
extern ProcessService*             g_processService;
extern FileOperationService*       g_fileOperationService;
extern PowerService*               g_powerService;
extern ScreenshotService*          g_screenshotService;
extern FileService*                g_fileService;
extern ClipboardService*           g_clipboardService;
extern WindowService*              g_windowService;
extern AppService*                 g_appService;
extern CommandService*             g_commandService;
extern BrowserService*             g_browserService;
extern ULONG_PTR                   g_gdiplusToken;

// HTTP 服务器
extern HANDLE g_serverThread;
extern SOCKET g_serverSocket;
extern DWORD  g_startTickCount;  // GetTickCount() at startup, for uptime calculation

// ── 共享工具函数 ────────────────────────────────────────
std::wstring GetLogDirW();
void         EnsureLogDir();
void         AppendCrashLog(const std::wstring& line);
void         AppendExceptionLogA(const std::string& line);
void         AppendHttpServerLogA(const std::string& line);

std::string  GetLocalIPAddress();
bool         AddFirewallRule();
bool         CheckFirewallRule();

std::string  UrlDecode(const std::string& str);
std::string  GetHeaderValue(const std::string& request, const std::string& headerNameLower);
std::string  BuildUrlFromRequest(const std::string& request, const std::string& path);
std::wstring ToWide(const std::string& value);

bool         IsAuthorizedRequest(const std::string& request);
std::string  MakeUnauthorizedResponse();

std::string  GetProcessList();
std::string  ExecuteCommand(const std::string& command);
std::string  CaptureScreenshot(const std::string& format = "png");

bool SaveClipboardImageFromOpenClipboard(std::string& outPath);
bool SaveClipboardFilesFromOpenClipboard(std::vector<std::string>& outPaths,
                                          std::vector<std::string>& outNames);
bool SaveScreenshotToFile(const std::string& format, std::string& outPath,
                           int& width, int& height);
int  GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

void UpdateTrayIconTooltip();
void CloseDashboardWindow();
void SignalDaemonExit();
std::wstring Localize(const char* key, const wchar_t* fallback);
std::string GetWindowList();
std::string RedactAuthorizationHeader(const std::string& request);
bool SendHotkey(const std::string& hotkey);
bool IsModifierKey(WORD vk);
std::string BuildHelpJson();

void RegisterMcpTools();

// HTTP 服务器
DWORD WINAPI HttpServerThread(LPVOID lpParam);
std::string  HandleHttpRequest(const std::string& request);

// MCP 协议
std::string HandleMCPInitialize(const std::string& body);
std::string HandleMCPToolsList();
std::string HandleMCPToolsCall(const std::string& body);

// 托盘窗口
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // CLAWDESK_APP_GLOBALS_H
