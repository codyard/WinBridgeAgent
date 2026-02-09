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
#include "services/app_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include <tlhelp32.h>
#include <sstream>
#include <algorithm>
#include <cctype>

AppService::AppService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

LaunchResult AppService::launchApp(const std::string& appName) {
    LaunchResult result{};
    result.success = false;
    result.pid = 0;

    if (policyGuard_ && !policyGuard_->isAppAllowed(appName)) {
        result.error = "App not allowed";
        return result;
    }

    std::string path = resolveAppPath(appName);
    if (path.empty()) {
        result.error = "App not found in allowed list";
        return result;
    }

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    char* cmdLine = _strdup(path.c_str());
    BOOL success = CreateProcessA(
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );
    free(cmdLine);

    if (!success) {
        result.error = "Failed to launch app";
        return result;
    }

    result.success = true;
    result.pid = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}

CloseResult AppService::closeApp(const std::string& appNameOrPid) {
    CloseResult result{};
    result.success = false;

    DWORD pid = 0;
    if (!appNameOrPid.empty() &&
        std::all_of(appNameOrPid.begin(), appNameOrPid.end(), ::isdigit)) {
        pid = static_cast<DWORD>(std::stoul(appNameOrPid));
    } else {
        if (policyGuard_ && !policyGuard_->isAppAllowed(appNameOrPid)) {
            result.error = "App not allowed";
            return result;
        }
        pid = findProcessByName(appNameOrPid);
    }

    if (pid == 0) {
        result.error = "Process not found";
        return result;
    }

    if (!terminateProcess(pid)) {
        result.error = "Failed to terminate process";
        return result;
    }

    result.success = true;
    return result;
}

std::string AppService::resolveAppPath(const std::string& appName) {
    if (!configManager_) {
        return "";
    }
    auto apps = configManager_->getAllowedApps();
    auto it = apps.find(appName);
    if (it == apps.end()) {
        return "";
    }
    return it->second;
}

bool AppService::terminateProcess(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        return false;
    }
    BOOL ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return ok != 0;
}

DWORD AppService::findProcessByName(const std::string& name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32First(snapshot, &entry)) {
        CloseHandle(snapshot);
        return 0;
    }

    DWORD foundPid = 0;
    do {
        if (_stricmp(entry.szExeFile, name.c_str()) == 0) {
            foundPid = entry.th32ProcessID;
            break;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return foundPid;
}
