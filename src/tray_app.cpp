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
#include "tray_app.h"
#include "app_globals.h"
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <thread>
#include "support/config_manager.h"
#include "support/audit_logger.h"
#include "support/license_manager.h"
#include "support/dashboard_window.h"
#include "support/settings_window.h"
#include "support/update_checker.h"
#include "support/localization_manager.h"
#include "policy/policy_guard.h"
#include "mcp/tool_registry.h"
#include "support/download_progress_window.h"

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                // 右键点击托盘图标
                if (g_auditLogger) {
                    clawdesk::AuditLogEntry entry;
                    entry.time = g_auditLogger->getCurrentTimestamp();
                    entry.tool = "tray_menu";
                    entry.risk = clawdesk::RiskLevel::Low;
                    entry.result = "Right-click on tray icon";
                    entry.duration_ms = 0;
                    g_auditLogger->logToolCall(entry);
                }
                
                POINT pt;
                GetCursorPos(&pt);
                
                HMENU hMenu = CreatePopupMenu();
                
                // 状态信息（禁用，仅显示）
                AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"ClawDesk MCP Server");
                
                // 获取配置信息
                if (g_configManager) {
                    int port = g_configManager->getServerPort();
                    std::wstring statusLabel = Localize("tray.status.running", L"Status: Running");
                    std::wstring portLabel = Localize("tray.port", L"Port");
                    std::wstring statusText = statusLabel + L" | " + portLabel + L": " + std::to_wstring(port);
                    AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, statusText.c_str());
                    
                    // 获取本机 IP 地址和监听地址
                    std::string listenAddr = g_configManager->getListenAddress();
                    std::string localIP = GetLocalIPAddress();
                    
                    std::wstring listenLabel = Localize("tray.listen", L"Listen");
                    if (listenAddr == "0.0.0.0") {
                        std::wstring allLabel = Localize("tray.listen.all", L"All interfaces");
                        std::wstring listenAddrW = ToWide(listenAddr);
                        std::wstring localIPW = ToWide(localIP);
                        std::wstring line = listenLabel + L": " + listenAddrW + L" (" + allLabel + L" - " + localIPW + L")";
                        AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, line.c_str());
                    } else {
                        std::wstring localLabel = Localize("tray.listen.local", L"Localhost only");
                        std::wstring listenAddrW = ToWide(listenAddr);
                        std::wstring line = listenLabel + L": " + listenAddrW + L" (" + localLabel + L")";
                        AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, line.c_str());
                    }
                    
                    std::string license = g_configManager->getLicenseKey();
                    if (license.empty()) {
                        AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
                                    Localize("tray.license.free", L"License: Free Edition").c_str());
                    } else {
                        AppendMenuW(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
                                    Localize("tray.license.pro", L"License: Professional").c_str());
                    }
                }
                
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_USAGE,
                            Localize("tray.usage", L"Usage Statistics").c_str());
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_LOGS,
                            Localize("tray.logs", L"View Logs").c_str());
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_DASHBOARD,
                            Localize("tray.dashboard", L"Dashboard").c_str());
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_CHECK_UPDATE,
                            Localize("tray.check_update", L"Check for Updates...").c_str());
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS,
                            Localize("tray.settings", L"Settings").c_str());
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE_LISTEN,
                            Localize("tray.toggle_listen", L"Toggle Listen Address").c_str());
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_CONFIG,
                            Localize("tray.open_config", L"Open Config").c_str());
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT,
                            Localize("tray.about", L"About").c_str());
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT,
                            Localize("tray.exit", L"Exit").c_str());
                
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_TRAY_EXIT:
                    g_running = false;
                    if (g_configManager) {
                        try {
                            g_configManager->setDaemonEnabled(false);
                            g_configManager->save();
                        } catch (...) {
                        }
                    }
                    SignalDaemonExit();
                    CloseDashboardWindow();
                    PostQuitMessage(0);
                    if (g_mainThreadId) {
                        PostThreadMessage(g_mainThreadId, WM_QUIT, 0, 0);
                    }
                    break;
                    
                case ID_TRAY_ABOUT:
                    MessageBox(hwnd, 
                        "ClawDesk MCP Server v" CLAWDESK_VERSION "\n\n"
                        "Windows MCP Server\n"
                        "Secure system operations for AI assistants\n\n"
                        "Copyright (C) 2026",
                        "About ClawDesk MCP Server",
                        MB_OK | MB_ICONINFORMATION);
                    break;
                    
                case ID_TRAY_USAGE:
                    MessageBox(hwnd,
                        "Usage Statistics\n\n"
                        "Today's Calls: 0 / 100\n"
                        "Screenshots: 0 / 20\n\n"
                        "License: Free Edition\n"
                        "Remaining Quota: 100 calls",
                        "Usage Statistics",
                        MB_OK | MB_ICONINFORMATION);
                    break;
                    
                case ID_TRAY_LOGS:
                    // 打开日志目录
                    ShellExecute(NULL, "open", "logs", NULL, NULL, SW_SHOW);
                    break;
                    
                case ID_TRAY_DASHBOARD:
                    // 显示/隐藏 Dashboard
                    if (g_dashboard) {
                        if (g_auditLogger) {
                            clawdesk::AuditLogEntry entry;
                            entry.time = g_auditLogger->getCurrentTimestamp();
                            entry.tool = "dashboard";
                            entry.risk = clawdesk::RiskLevel::Low;
                            entry.duration_ms = 0;
                            
                            if (!g_dashboard->getHandle()) {
                                entry.result = "Creating dashboard window";
                                g_auditLogger->logToolCall(entry);
                                g_dashboard->create();
                                entry.result = "Dashboard window created";
                                g_auditLogger->logToolCall(entry);
                            } else if (g_dashboard->isVisible()) {
                                entry.result = "Hiding dashboard";
                                g_auditLogger->logToolCall(entry);
                                g_dashboard->hide();
                            } else {
                                entry.result = "Showing dashboard";
                                g_auditLogger->logToolCall(entry);
                                g_dashboard->show();
                                entry.result = "Dashboard shown";
                                g_auditLogger->logToolCall(entry);
                            }
                        } else {
                            // 没有 AuditLogger，直接操作
                            if (!g_dashboard->getHandle()) {
                                g_dashboard->create();
                            } else if (g_dashboard->isVisible()) {
                                g_dashboard->hide();
                            } else {
                                g_dashboard->show();
                            }
                        }
                    }
                    break;

                case ID_TRAY_SETTINGS:
                    if (!g_settingsWindow) {
                        g_settingsWindow = new SettingsWindow(
                            g_hInstance,
                            g_hwnd,
                            g_configManager,
                            g_licenseManager,
                            g_auditLogger,
                            g_localizationManager
                        );
                    }
                    if (g_settingsWindow) {
                        g_settingsWindow->Show();
                    }
                    break;
                    
                case ID_TRAY_CONFIG:
                    // 用记事本打开配置文件
                    ShellExecute(NULL, "open", "notepad.exe", "config.json", NULL, SW_SHOW);
                    break;
                
                case ID_TRAY_CHECK_UPDATE:
                #ifndef CLAWDESK_OPENSSL_ENABLED
                    MessageBoxW(hwnd,
                        Localize("update.https_disabled_message", L"HTTPS is not available because OpenSSL is disabled.\n\nPlease install a build with OpenSSL enabled to use update checks.").c_str(),
                        Localize("update.https_disabled_title", L"Check for Updates").c_str(),
                        MB_OK | MB_ICONINFORMATION);
                    break;
                #endif
                    // 检查更新
                    if (g_configManager && !g_configManager->isAutoUpdateEnabled()) {
                        MessageBoxW(hwnd,
                            L"Auto-update is disabled in configuration.\n\nPlease enable it in config.json to check for updates.",
                            Localize("update.title", L"Check for Updates").c_str(),
                            MB_OK | MB_ICONINFORMATION);
                        break;
                    }
                    
                    if (g_checkingUpdate.load()) {
                        MessageBoxW(hwnd, 
                            Localize("update.already_checking", L"Already checking for updates...").c_str(),
                            Localize("update.title", L"Check for Updates").c_str(),
                            MB_OK | MB_ICONINFORMATION);
                        break;
                    }
                    
                    if (g_updateChecker && g_configManager) {
                        g_checkingUpdate.store(true);
                        
                        // 异步检查更新，使用 PostMessage 将结果发送到 UI 线程
                        g_updateChecker->checkForUpdatesAsync([hwnd](const clawdesk::UpdateCheckResult& result) {
                            // 在后台线程中，不直接调用 MessageBox
                            // 而是将结果保存并通过 PostMessage 通知 UI 线程
                            
                            // 分配结果副本（UI 线程负责释放）
                            clawdesk::UpdateCheckResult* resultCopy = new clawdesk::UpdateCheckResult(result);
                            
                            // 发送消息到 UI 线程
                            PostMessage(hwnd, WM_UPDATE_CHECK_RESULT, 0, reinterpret_cast<LPARAM>(resultCopy));
                        }, g_configManager->getUpdateChannel() == "beta");
                    }
                    break;
                    
                case ID_TRAY_TOGGLE_LISTEN:
                    // 切换监听地址
                    if (g_configManager) {
                        std::string currentAddr = g_configManager->getListenAddress();
                        std::string newAddr = (currentAddr == "0.0.0.0") ? "127.0.0.1" : "0.0.0.0";
                        g_configManager->setListenAddress(newAddr);
                        g_configManager->save();
                        
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                                 "Listen address changed to %s\n\nPlease restart the server for changes to take effect.",
                                 newAddr.c_str());
                        MessageBox(hwnd, msg, "Listen Address Changed", MB_OK | MB_ICONINFORMATION);
                    }
                    break;
            }
            break;
            
        case WM_EXIT_COMMAND:
            // 收到退出命令
            g_running = false;
            SignalDaemonExit();  // 通知 Daemon 退出
            CloseDashboardWindow();
            PostQuitMessage(0);
            if (g_mainThreadId) {
                PostThreadMessage(g_mainThreadId, WM_QUIT, 0, 0);
            }
            break;
            
        case WM_UPDATE_CHECK_RESULT:
            // 处理更新检查结果（在 UI 线程中）
            {
                g_checkingUpdate.store(false);
                
                // 获取结果指针
                clawdesk::UpdateCheckResult* result = reinterpret_cast<clawdesk::UpdateCheckResult*>(lParam);
                if (!result) break;
                
                // 使用智能指针自动释放内存
                std::unique_ptr<clawdesk::UpdateCheckResult> resultPtr(result);
                
                if (!result->success) {
                    // 只记录日志，不显示弹框
                    if (g_dashboard) {
                        g_dashboard->logError("auto_update", "Failed to check for updates: " + result->errorMessage);
                    }
                    break;
                }
                
                if (!result->updateAvailable) {
                    std::wstring msg = Localize("update.up_to_date", L"You are running the latest version: ") +
                                      std::wstring(result->currentVersion.toString().begin(), 
                                                  result->currentVersion.toString().end());
                    MessageBoxW(hwnd, msg.c_str(),
                        Localize("update.title", L"Check for Updates").c_str(),
                        MB_OK | MB_ICONINFORMATION);
                    break;
                }
                
                // 检查是否是已跳过的版本
                if (g_configManager) {
                    std::string skippedVersion = g_configManager->getSkippedVersion();
                    std::string latestVersion = result->latestRelease.version.toString();
                    
                    if (!skippedVersion.empty() && skippedVersion == latestVersion) {
                        // 这是已跳过的版本，不显示更新提示
                        break;
                    }
                }
                
                // 有新版本可用
                std::wstring currentVer(result->currentVersion.toString().begin(), 
                                       result->currentVersion.toString().end());
                std::wstring latestVer(result->latestRelease.version.toString().begin(),
                                      result->latestRelease.version.toString().end());
                std::wstring releaseName(result->latestRelease.name.begin(),
                                        result->latestRelease.name.end());
                
                std::wstring msg = Localize("update.available", L"A new version is available!\n\n") +
                                 L"Current: " + currentVer + L"\n" +
                                 L"Latest: " + latestVer + L"\n\n" +
                                 releaseName + L"\n\n" +
                                 L"Do you want to download and install the update now?\n\n" +
                                 L"[Yes] Download and install\n" +
                                 L"[No] Visit GitHub\n" +
                                 L"[Cancel] Skip this version";
                
                int response = MessageBoxW(hwnd, msg.c_str(),
                    Localize("update.title", L"Check for Updates").c_str(),
                    MB_YESNOCANCEL | MB_ICONINFORMATION);
                
                if (response == IDYES) {
                    // 下载并安装更新
                    if (result->matchedAsset.downloadUrl.empty()) {
                        MessageBoxW(hwnd, L"No download URL available.", L"Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // 获取下载路径（临时目录）
                    char tempPath[MAX_PATH];
                    GetTempPathA(MAX_PATH, tempPath);
                    std::string downloadPath = std::string(tempPath) + result->matchedAsset.name;
                    
                    // 创建进度窗口
                    clawdesk::DownloadProgressWindow* progressWnd = new clawdesk::DownloadProgressWindow();
                    progressWnd->create();
                    progressWnd->setTitle("Downloading Update");
                    progressWnd->show();
                    
                    // 设置取消回调
                    progressWnd->setCancelCallback([&]() {
                        if (g_updateChecker) {
                            g_updateChecker->cancelDownload();
                        }
                    });
                    
                    // 异步下载
                    if (g_updateChecker) {
                        g_updateChecker->downloadUpdateAsync(
                            *result,
                            downloadPath,
                            [hwnd, progressWnd, downloadPath](bool success, const std::string& errorMessage) {
                                // 创建完成信息结构（在 UI 线程中释放）
                                struct DownloadCompleteInfo {
                                    bool success;
                                    std::string errorMessage;
                                    std::string downloadPath;
                                    clawdesk::DownloadProgressWindow* progressWnd;
                                };
                                
                                DownloadCompleteInfo* info = new DownloadCompleteInfo{
                                    success, errorMessage, downloadPath, progressWnd
                                };
                                
                                // 使用 PostMessage 将完成事件发送到 UI 线程
                                PostMessage(hwnd, WM_DOWNLOAD_COMPLETE, 0, reinterpret_cast<LPARAM>(info));
                            },
                            [progressWnd](const clawdesk::DownloadProgress& progress) {
                                if (progressWnd) {
                                    progressWnd->updateProgress(progress);
                                }
                            }
                        );
                    }
                } else if (response == IDNO) {
                    // 打开 GitHub Release 页面
                    ShellExecuteA(NULL, "open", result->latestRelease.htmlUrl.c_str(), 
                                NULL, NULL, SW_SHOW);
                } else if (response == IDCANCEL) {
                    // 跳过此版本
                    if (g_configManager) {
                        std::string latestVersion = result->latestRelease.version.toString();
                        g_configManager->setSkippedVersion(latestVersion);
                        g_configManager->save();
                        
                        std::wstring msg = L"Version " + latestVer + L" will be skipped.\n\n" +
                                         L"You can reset this in config.json if needed.";
                        MessageBoxW(hwnd, msg.c_str(),
                            Localize("update.title", L"Check for Updates").c_str(),
                            MB_OK | MB_ICONINFORMATION);
                    }
                }
                
                // 更新最后检查时间
                if (g_configManager) {
                    auto now = std::chrono::system_clock::now();
                    auto time_t_now = std::chrono::system_clock::to_time_t(now);
                    std::tm tm_now;
                    gmtime_s(&tm_now, &time_t_now);
                    char timestamp[32];
                    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm_now);
                    g_configManager->setLastUpdateCheck(timestamp);
                    g_configManager->save();
                }
            }
            break;
        
        case WM_DOWNLOAD_COMPLETE:
            // 处理下载完成事件（在 UI 线程中）
            {
                struct DownloadCompleteInfo {
                    bool success;
                    std::string errorMessage;
                    std::string downloadPath;
                    clawdesk::DownloadProgressWindow* progressWnd;
                };
                
                DownloadCompleteInfo* info = reinterpret_cast<DownloadCompleteInfo*>(lParam);
                if (!info) break;
                
                // 使用智能指针自动释放内存
                std::unique_ptr<DownloadCompleteInfo> infoPtr(info);
                
                // 关闭并删除进度窗口
                if (info->progressWnd) {
                    info->progressWnd->close();
                    delete info->progressWnd;
                }
                
                if (!info->success) {
                    std::wstring msg = L"Download failed: " + 
                                      std::wstring(info->errorMessage.begin(), info->errorMessage.end());
                    MessageBoxW(hwnd, msg.c_str(), L"Update Failed", MB_OK | MB_ICONERROR);
                    break;
                }
                
                // 下载成功，询问是否立即安装
                int installResponse = MessageBoxW(hwnd,
                    L"Download completed successfully!\n\nInstall now and restart the application?",
                    L"Update Ready",
                    MB_YESNO | MB_ICONQUESTION);
                
                if (installResponse == IDYES && g_updateChecker) {
                    SignalDaemonExit();
                    // 获取当前程序路径
                    char exePath[MAX_PATH];
                    GetModuleFileNameA(NULL, exePath, MAX_PATH);
                    
                    // 启动升级器
                    if (g_updateChecker->startUpdater(info->downloadPath, exePath)) {
                        // 退出主程序
                        PostMessage(hwnd, WM_CLOSE, 0, 0);
                    } else {
                        MessageBoxW(hwnd,
                            L"Failed to start updater. Please install manually.",
                            L"Update Error",
                            MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;
            
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// 创建托盘图标
bool CreateTrayIcon(HWND hwnd) {
    memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    std::wstring tip = Localize("tray.tip", L"ClawDesk MCP Server - Running");
    size_t maxLen = sizeof(nid.szTip) / sizeof(nid.szTip[0]);
    // Convert wide string to ANSI for szTip
    WideCharToMultiByte(CP_ACP, 0, tip.c_str(), -1, nid.szTip, maxLen, NULL, NULL);
    
    return Shell_NotifyIcon(NIM_ADD, &nid);
}

void UpdateTrayIconTooltip() {
    if (!g_hwnd) {
        return;
    }
    NOTIFYICONDATA update{};
    update.cbSize = sizeof(update);
    update.hWnd = g_hwnd;
    update.uID = 1;
    update.uFlags = NIF_TIP;
    std::wstring tip = Localize("tray.tip", L"ClawDesk MCP Server - Running");
    size_t maxLen = sizeof(update.szTip) / sizeof(update.szTip[0]);
    // Convert wide string to ANSI for szTip
    WideCharToMultiByte(CP_ACP, 0, tip.c_str(), -1, update.szTip, maxLen, NULL, NULL);
    Shell_NotifyIcon(NIM_MODIFY, &update);
}
