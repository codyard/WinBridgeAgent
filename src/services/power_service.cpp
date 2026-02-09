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
#include "services/power_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <powrprof.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

// Link with powrprof.lib for SetSuspendState
#pragma comment(lib, "powrprof.lib")

// ===== 构造函数 =====

PowerService::PowerService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

// ===== 公共方法 =====

ShutdownResult PowerService::shutdownSystem(PowerAction action,
                                           int delay,
                                           bool force,
                                           const std::string& message) {
    ShutdownResult result;
    result.action = action;
    result.delay = delay;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args;
        std::string actionStr = "shutdown";
        if (action == PowerAction::Reboot) actionStr = "reboot";
        else if (action == PowerAction::Hibernate) actionStr = "hibernate";
        else if (action == PowerAction::Sleep) actionStr = "sleep";
        args["action"] = actionStr;
        args["delay"] = delay;
        args["force"] = force;
        if (!message.empty()) {
            args["message"] = message;
        }
        auto decision = policyGuard_->evaluateToolCall("shutdown_system", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 启用关机权限
    if (!enableShutdownPrivilege()) {
        result.error = "Failed to enable shutdown privilege (administrator privileges may be required)";
        return result;
    }

    // 2. 根据操作类型执行相应的操作
    bool operationSuccess = false;
    
    if (action == PowerAction::Shutdown || action == PowerAction::Reboot) {
        // 关机或重启
        operationSuccess = initiateShutdown(action, delay, force, message);
        
        if (operationSuccess) {
            // 计算计划时间
            result.scheduledTime = formatScheduledTime(delay);
            
            std::cout << "System " 
                      << (action == PowerAction::Shutdown ? "shutdown" : "reboot")
                      << " scheduled for " << result.scheduledTime
                      << " (delay: " << delay << " seconds)" << std::endl;
        }
    } else if (action == PowerAction::Hibernate || action == PowerAction::Sleep) {
        // 休眠或睡眠
        operationSuccess = initiateSuspend(action);
        
        if (operationSuccess) {
            result.scheduledTime = formatScheduledTime(0); // 立即执行
            
            std::cout << "System " 
                      << (action == PowerAction::Hibernate ? "hibernate" : "sleep")
                      << " initiated" << std::endl;
        }
    } else {
        result.error = "Unknown power action";
        return result;
    }

    // 3. 检查操作结果
    if (!operationSuccess) {
        DWORD error = GetLastError();
        if (error == ERROR_PRIVILEGE_NOT_HELD) {
            result.error = "Privilege not held (administrator privileges required)";
        } else {
            result.error = "Failed to initiate power operation (error code: " + std::to_string(error) + ")";
        }
        return result;
    }

    // 4. 成功
    result.success = true;
    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("shutdown_system");
    }
    return result;
}

AbortShutdownResult PowerService::abortShutdown() {
    AbortShutdownResult result;
    result.success = false;

    if (policyGuard_) {
        nlohmann::json args = nlohmann::json::object();
        auto decision = policyGuard_->evaluateToolCall("abort_shutdown", args);
        if (!decision.allowed) {
            result.error = decision.reason;
            return result;
        }
    }

    // 1. 启用关机权限
    if (!enableShutdownPrivilege()) {
        result.error = "Failed to enable shutdown privilege (administrator privileges may be required)";
        return result;
    }

    // 2. 取消关机
    if (!AbortSystemShutdown(NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_SHUTDOWN_IN_PROGRESS) {
            result.error = "No shutdown in progress";
        } else if (error == ERROR_PRIVILEGE_NOT_HELD) {
            result.error = "Privilege not held (administrator privileges required)";
        } else {
            result.error = "Failed to abort shutdown (error code: " + std::to_string(error) + ")";
        }
        return result;
    }

    // 3. 成功
    result.success = true;
    result.message = "Shutdown cancelled successfully";
    
    std::cout << "System shutdown cancelled" << std::endl;
    
    if (result.success && policyGuard_) {
        policyGuard_->incrementUsageCount("abort_shutdown");
    }
    return result;
}

// ===== 私有方法 =====

bool PowerService::enableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // 1. 打开进程令牌
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        std::cerr << "OpenProcessToken failed: " << GetLastError() << std::endl;
        return false;
    }

    // 2. 查找 SE_SHUTDOWN_NAME 权限的 LUID
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        std::cerr << "LookupPrivilegeValue failed: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return false;
    }

    // 3. 设置权限属性
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 4. 调整令牌权限
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL)) {
        std::cerr << "AdjustTokenPrivileges failed: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return false;
    }

    // 5. 检查是否成功（AdjustTokenPrivileges 可能返回 TRUE 但实际失败）
    DWORD error = GetLastError();
    if (error != ERROR_SUCCESS) {
        std::cerr << "AdjustTokenPrivileges error: " << error << std::endl;
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool PowerService::initiateShutdown(PowerAction action, int delay, bool force, const std::string& message) {
    // 将 std::string 转换为 LPSTR（Windows API 需要）
    LPSTR messageStr = const_cast<LPSTR>(message.empty() ? NULL : message.c_str());
    
    // reboot = true 表示重启，false 表示关机
    BOOL reboot = (action == PowerAction::Reboot) ? TRUE : FALSE;
    
    // 调用 InitiateSystemShutdownEx
    // 参数：
    // - lpMachineName: NULL 表示本地计算机
    // - lpMessage: 显示给用户的消息
    // - dwTimeout: 延迟时间（秒）
    // - bForceAppsClosed: 是否强制关闭应用程序
    // - bRebootAfterShutdown: 是否重启
    BOOL result = InitiateSystemShutdownEx(
        NULL,           // 本地计算机
        messageStr,     // 消息
        delay,          // 延迟时间
        force,          // 强制关闭应用程序
        reboot,         // 是否重启
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED
    );

    return (result != 0);
}

bool PowerService::initiateSuspend(PowerAction action) {
    // hibernate = true 表示休眠，false 表示睡眠
    BOOLEAN hibernate = (action == PowerAction::Hibernate) ? TRUE : FALSE;
    
    // 调用 SetSuspendState
    // 参数：
    // - bHibernate: 是否休眠（TRUE）或睡眠（FALSE）
    // - bForce: 是否强制挂起（通常设置为 FALSE）
    // - bWakeupEventsDisabled: 是否禁用唤醒事件（通常设置为 FALSE）
    BOOLEAN result = SetSuspendState(
        hibernate,  // 休眠或睡眠
        FALSE,      // 不强制挂起
        FALSE       // 不禁用唤醒事件
    );

    return (result != 0);
}

std::string PowerService::formatScheduledTime(int delay) {
    // 1. 获取当前时间
    auto now = std::chrono::system_clock::now();
    
    // 2. 添加延迟
    auto scheduledTime = now + std::chrono::seconds(delay);
    
    // 3. 转换为 time_t
    std::time_t scheduledTimeT = std::chrono::system_clock::to_time_t(scheduledTime);
    
    // 4. 格式化为 ISO 8601 格式（UTC）
    std::tm* tm = std::gmtime(&scheduledTimeT);
    
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
    
    return oss.str();
}
