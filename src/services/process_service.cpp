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
#include "services/process_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cctype>
#include <iostream>

// ===== 静态成员初始化 =====

const std::vector<std::string> ProcessService::PROTECTED_PROCESSES = {
    "system",
    "csrss.exe",
    "winlogon.exe",
    "services.exe",
    "lsass.exe",
    "smss.exe",
    "wininit.exe"
};

// ===== 辅助结构体（用于窗口枚举） =====

struct EnumWindowsData {
    DWORD pid;
    std::vector<HWND> windows;
};

// ===== 辅助函数 =====

// 窗口枚举回调函数
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumWindowsData* data = reinterpret_cast<EnumWindowsData*>(lParam);
    
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    
    if (windowPid == data->pid && IsWindowVisible(hwnd)) {
        data->windows.push_back(hwnd);
    }
    
    return TRUE;
}

// 字符串转小写
static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ===== 构造函数 =====

ProcessService::ProcessService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

// ===== 公共方法 =====

KillProcessResult ProcessService::killProcess(DWORD pid, bool force) {
    KillProcessResult result;
    result.pid = pid;
    result.forced = force;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        args["pid"] = pid;
        args["force"] = force;
        auto decision = policyGuard_->evaluateToolCall("kill_process", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 获取进程名称
    result.processName = getProcessName(pid);
    if (result.processName.empty()) {
        result.error = "Process not found or access denied";
        return result;
    }

    // 2. 检查是否为受保护进程
    if (isProtectedProcess(toLower(result.processName))) {
        result.error = "Cannot terminate protected system process: " + result.processName;
        return result;
    }

    // 3. 尝试终止进程
    if (!force) {
        // 先尝试优雅关闭
        std::cout << "Attempting graceful close for PID " << pid << " (" << result.processName << ")..." << std::endl;
        if (tryGracefulClose(pid)) {
            result.success = true;
            result.forced = false;
            std::cout << "Process terminated gracefully" << std::endl;
            return result;
        }
        std::cout << "Graceful close failed, falling back to force terminate" << std::endl;
    }

    // 4. 强制终止
    if (forceTerminate(pid)) {
        result.success = true;
        result.forced = true;
        std::cout << "Process terminated forcefully" << std::endl;
        return result;
    }

    result.error = "Failed to terminate process";
    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("kill_process");
    }
    return result;
}

SetPriorityResult ProcessService::setProcessPriority(DWORD pid, ProcessPriority priority) {
    SetPriorityResult result;
    result.pid = pid;
    result.newPriority = priority;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        args["pid"] = pid;
        args["priority"] = priorityToString(priority);
        auto decision = policyGuard_->evaluateToolCall("set_process_priority", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 打开进程句柄
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            result.error = "Access denied (administrator privileges may be required)";
        } else if (error == ERROR_INVALID_PARAMETER) {
            result.error = "Process not found";
        } else {
            result.error = "Failed to open process (error code: " + std::to_string(error) + ")";
        }
        return result;
    }

    // 2. 获取当前优先级
    DWORD currentPriority = GetPriorityClass(hProcess);
    if (currentPriority == 0) {
        result.error = "Failed to get current priority";
        CloseHandle(hProcess);
        return result;
    }
    result.oldPriority = mapWin32ToPriority(currentPriority);

    // 3. 设置新优先级
    DWORD newPriorityClass = mapPriorityToWin32(priority);
    if (!SetPriorityClass(hProcess, newPriorityClass)) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            result.error = "Access denied (realtime priority requires administrator privileges)";
        } else {
            result.error = "Failed to set priority (error code: " + std::to_string(error) + ")";
        }
        CloseHandle(hProcess);
        return result;
    }

    // 4. 成功
    result.success = true;
    CloseHandle(hProcess);

    std::cout << "Process priority changed: PID " << pid 
              << " from " << static_cast<int>(result.oldPriority)
              << " to " << static_cast<int>(result.newPriority) << std::endl;

    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("set_process_priority");
    }
    return result;
}

// ===== 私有方法 =====

bool ProcessService::isProtectedProcess(const std::string& processName) {
    std::string lowerName = toLower(processName);
    
    for (const auto& protectedName : PROTECTED_PROCESSES) {
        if (lowerName == protectedName || lowerName.find(protectedName) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool ProcessService::tryGracefulClose(DWORD pid) {
    // 1. 枚举进程的所有窗口
    EnumWindowsData data;
    data.pid = pid;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

    if (data.windows.empty()) {
        // 没有窗口，无法优雅关闭
        return false;
    }

    // 2. 向所有窗口发送 WM_CLOSE 消息
    for (HWND hwnd : data.windows) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }

    // 3. 等待进程退出（最多 5 秒）
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(hProcess, 5000); // 5 秒超时
    CloseHandle(hProcess);

    return (waitResult == WAIT_OBJECT_0);
}

bool ProcessService::forceTerminate(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) {
        return false;
    }

    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);

    return (result != 0);
}

std::string ProcessService::getProcessName(DWORD pid) {
    // 特殊处理：PID 0 和 4 是系统进程
    if (pid == 0) {
        return "System Idle Process";
    }
    if (pid == 4) {
        return "System";
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return "";
    }

    char processPath[MAX_PATH];
    DWORD size = MAX_PATH;

    // 尝试使用 QueryFullProcessImageName（Windows Vista+）
    if (QueryFullProcessImageNameA(hProcess, 0, processPath, &size)) {
        CloseHandle(hProcess);
        
        // 提取文件名
        std::string fullPath(processPath);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }

    // 回退：尝试使用 GetModuleFileNameEx
    if (GetModuleFileNameExA(hProcess, NULL, processPath, MAX_PATH)) {
        CloseHandle(hProcess);
        
        // 提取文件名
        std::string fullPath(processPath);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }

    CloseHandle(hProcess);
    return "";
}

DWORD ProcessService::mapPriorityToWin32(ProcessPriority priority) {
    switch (priority) {
        case ProcessPriority::Idle:
            return IDLE_PRIORITY_CLASS;
        case ProcessPriority::BelowNormal:
            return BELOW_NORMAL_PRIORITY_CLASS;
        case ProcessPriority::Normal:
            return NORMAL_PRIORITY_CLASS;
        case ProcessPriority::AboveNormal:
            return ABOVE_NORMAL_PRIORITY_CLASS;
        case ProcessPriority::High:
            return HIGH_PRIORITY_CLASS;
        case ProcessPriority::Realtime:
            return REALTIME_PRIORITY_CLASS;
        default:
            return NORMAL_PRIORITY_CLASS;
    }
}

ProcessPriority ProcessService::mapWin32ToPriority(DWORD win32Priority) {
    switch (win32Priority) {
        case IDLE_PRIORITY_CLASS:
            return ProcessPriority::Idle;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return ProcessPriority::BelowNormal;
        case NORMAL_PRIORITY_CLASS:
            return ProcessPriority::Normal;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return ProcessPriority::AboveNormal;
        case HIGH_PRIORITY_CLASS:
            return ProcessPriority::High;
        case REALTIME_PRIORITY_CLASS:
            return ProcessPriority::Realtime;
        default:
            return ProcessPriority::Normal;
    }
}

// ===== 静态辅助方法 =====

std::string ProcessService::priorityToString(ProcessPriority priority) {
    switch (priority) {
        case ProcessPriority::Idle:
            return "idle";
        case ProcessPriority::BelowNormal:
            return "below_normal";
        case ProcessPriority::Normal:
            return "normal";
        case ProcessPriority::AboveNormal:
            return "above_normal";
        case ProcessPriority::High:
            return "high";
        case ProcessPriority::Realtime:
            return "realtime";
        default:
            return "normal";
    }
}
