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
#include <windows.h>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>
#include "support/config_manager.h"
#include "utils/log_path.h"

#define CLAWDESK_MAIN_MUTEX_NAME L"WinBridgeAgentServer_SingleInstance"
#define CLAWDESK_DAEMON_MUTEX_NAME L"WinBridgeAgentDaemon_SingleInstance"
#define CLAWDESK_DAEMON_EXIT_EVENT L"WinBridgeAgentDaemon_Exit"

namespace {
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

void EnsureLogDir() {
    clawdesk::EnsureLogDir();
}

void LogLine(const std::string& line) {
    EnsureLogDir();
    std::ofstream out(clawdesk::GetLogFilePathA("daemon.log"), std::ios::app);
    if (!out.is_open()) {
        return;
    }
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    out << "[" << buf << "] " << line << "\n";
}

bool IsMainRunning() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, CLAWDESK_MAIN_MUTEX_NAME);
    if (mutex) {
        CloseHandle(mutex);
        return true;
    }
    return false;
}

bool IsDaemonEnabled() {
    try {
        ConfigManager cfg;
        cfg.load();
        return cfg.isDaemonEnabled();
    } catch (...) {
        LogLine("Failed to load config, keep daemon enabled");
        return true;
    }
}

bool StartMainProcess() {
    std::wstring dir = GetExeDirW();
    if (dir.empty()) {
        LogLine("Cannot resolve executable directory");
        return false;
    }
    
    // 确定架构后缀
    std::wstring arch;
#if defined(_M_X64) || defined(__x86_64__)
    arch = L"-x64";
#elif defined(_M_IX86) || defined(__i386__)
    arch = L"-x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    arch = L"-arm64";
#else
    arch = L"";
#endif
    
    std::wstring exePath = dir + L"\\WinBridgeAgent" + arch + L".exe";
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    BOOL ok = CreateProcessW(exePath.c_str(), NULL, NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, dir.c_str(), &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        std::string msg = "Started main process: WinBridgeAgent";
        if (!arch.empty()) {
            msg += std::string(arch.begin(), arch.end());
        }
        msg += ".exe";
        LogLine(msg);
        return true;
    }
    DWORD err = GetLastError();
    std::ostringstream oss;
    oss << "Failed to start main process, error=" << err;
    LogLine(oss.str());
    return false;
}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE daemonMutex = CreateMutexW(NULL, TRUE, CLAWDESK_DAEMON_MUTEX_NAME);
    if (!daemonMutex) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(daemonMutex);
        return 0;
    }

    std::wstring dir = GetExeDirW();
    if (!dir.empty()) {
        SetCurrentDirectoryW(dir.c_str());
    }

    HANDLE exitEvent = CreateEventW(NULL, TRUE, FALSE, CLAWDESK_DAEMON_EXIT_EVENT);
    if (!exitEvent) {
        CloseHandle(daemonMutex);
        return 1;
    }
    ResetEvent(exitEvent);

    LogLine("Daemon started");

    const int kCheckIntervalMs = 1000;
    const int kRestartDelayMs = 10000;

    while (true) {
        DWORD wait = WaitForSingleObject(exitEvent, kCheckIntervalMs);
        if (wait == WAIT_OBJECT_0) {
            LogLine("Exit event received");
            break;
        }

        if (!IsDaemonEnabled()) {
            LogLine("Daemon disabled by config, exiting");
            break;
        }

        if (!IsMainRunning()) {
            int waited = 0;
            while (waited < kRestartDelayMs) {
                DWORD w = WaitForSingleObject(exitEvent, kCheckIntervalMs);
                if (w == WAIT_OBJECT_0) {
                    LogLine("Exit event received during restart delay");
                    goto exit_loop;
                }
                if (!IsDaemonEnabled()) {
                    LogLine("Daemon disabled during restart delay");
                    goto exit_loop;
                }
                if (IsMainRunning()) {
                    break;
                }
                waited += kCheckIntervalMs;
            }

            if (!IsMainRunning() && IsDaemonEnabled()) {
                StartMainProcess();
            }
        }
    }

exit_loop:
    CloseHandle(exitEvent);
    CloseHandle(daemonMutex);
    return 0;
}
