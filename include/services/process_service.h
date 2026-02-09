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
#ifndef PROCESS_SERVICE_H
#define PROCESS_SERVICE_H

#include <string>
#include <vector>
#include <windows.h>

// Forward declarations
class ConfigManager;

// PolicyGuard is optional - will be integrated when available
class PolicyGuard;

/**
 * ProcessPriority - 进程优先级枚举
 * 
 * 映射到 Windows 进程优先级类：
 * - Idle: IDLE_PRIORITY_CLASS
 * - BelowNormal: BELOW_NORMAL_PRIORITY_CLASS
 * - Normal: NORMAL_PRIORITY_CLASS
 * - AboveNormal: ABOVE_NORMAL_PRIORITY_CLASS
 * - High: HIGH_PRIORITY_CLASS
 * - Realtime: REALTIME_PRIORITY_CLASS (需要管理员权限)
 */
enum class ProcessPriority {
    Idle,
    BelowNormal,
    Normal,
    AboveNormal,
    High,
    Realtime
};

/**
 * KillProcessResult - 进程终止结果
 * 
 * 包含进程终止操作的结果信息：
 * - success: 操作是否成功
 * - pid: 进程 ID
 * - processName: 进程名称
 * - forced: 是否使用强制终止
 * - error: 错误信息（如果失败）
 */
struct KillProcessResult {
    bool success;
    DWORD pid;
    std::string processName;
    bool forced;
    std::string error;
};

/**
 * SetPriorityResult - 进程优先级调整结果
 * 
 * 包含优先级调整操作的结果信息：
 * - success: 操作是否成功
 * - pid: 进程 ID
 * - oldPriority: 调整前的优先级
 * - newPriority: 调整后的优先级
 * - error: 错误信息（如果失败）
 */
struct SetPriorityResult {
    bool success;
    DWORD pid;
    ProcessPriority oldPriority;
    ProcessPriority newPriority;
    std::string error;
};

/**
 * ProcessService - 进程管理服务
 * 
 * 提供进程终止和优先级调整功能。
 * 
 * 功能：
 * - 终止进程（优雅关闭 + 强制终止）
 * - 调整进程优先级
 * - 受保护进程黑名单检查
 * - 进程名称获取
 * 
 * 需求覆盖：
 * - 25.1: 终止指定 PID 的进程
 * - 25.2: 优雅关闭（WM_CLOSE）和强制终止（TerminateProcess）
 * - 25.3: 强制终止模式
 * - 25.4: 禁止终止受保护的系统进程
 * - 25.5: 返回进程 ID 和进程名称
 * - 25.6: 记录操作到审计日志
 * - 26.1: 调整指定进程的优先级
 * - 26.2: 支持多种优先级（idle, below_normal, normal, above_normal, high, realtime）
 * - 26.3: realtime 优先级需要管理员权限
 * - 26.4: 返回旧优先级和新优先级
 * - 26.5: 记录操作到审计日志
 */
class ProcessService {
public:
    /**
     * 构造函数
     * @param configManager 配置管理器指针
     * @param policyGuard 策略守卫指针（可选，可为 nullptr）
     */
    ProcessService(ConfigManager* configManager, PolicyGuard* policyGuard);

    /**
     * 终止进程
     * 
     * @param pid 进程 ID
     * @param force 是否强制终止（true: 直接 TerminateProcess, false: 先尝试 WM_CLOSE）
     * @return KillProcessResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 检查进程是否存在
     * 2. 获取进程名称
     * 3. 检查是否为受保护进程
     * 4. 如果 force=false，先尝试优雅关闭（发送 WM_CLOSE 到所有窗口）
     * 5. 如果优雅关闭失败或 force=true，使用 TerminateProcess 强制终止
     * 6. 返回结果
     */
    KillProcessResult killProcess(DWORD pid, bool force);

    /**
     * 调整进程优先级
     * 
     * @param pid 进程 ID
     * @param priority 目标优先级
     * @return SetPriorityResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 打开进程句柄（PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION）
     * 2. 获取当前优先级
     * 3. 设置新优先级
     * 4. 返回结果（包含旧优先级和新优先级）
     */
    SetPriorityResult setProcessPriority(DWORD pid, ProcessPriority priority);

    /**
     * 将优先级枚举转换为字符串
     * 
     * @param priority ProcessPriority 枚举值
     * @return 优先级字符串（如 "normal", "high"）
     */
    static std::string priorityToString(ProcessPriority priority);

private:
    /**
     * 检查进程是否受保护
     * 
     * 受保护进程列表（硬编码）：
     * - system
     * - csrss.exe
     * - winlogon.exe
     * - services.exe
     * - lsass.exe
     * - smss.exe
     * - wininit.exe
     * 
     * @param processName 进程名称（小写）
     * @return true 表示进程受保护
     */
    bool isProtectedProcess(const std::string& processName);

    /**
     * 尝试优雅关闭进程
     * 
     * 实现逻辑：
     * 1. 枚举进程的所有窗口
     * 2. 向每个窗口发送 WM_CLOSE 消息
     * 3. 等待进程退出（最多 5 秒）
     * 4. 返回是否成功
     * 
     * @param pid 进程 ID
     * @return true 表示进程已退出
     */
    bool tryGracefulClose(DWORD pid);

    /**
     * 强制终止进程
     * 
     * 使用 TerminateProcess API 强制终止进程
     * 
     * @param pid 进程 ID
     * @return true 表示终止成功
     */
    bool forceTerminate(DWORD pid);

    /**
     * 获取进程名称
     * 
     * 使用 QueryFullProcessImageName 或 GetModuleFileNameEx 获取进程路径，
     * 然后提取文件名
     * 
     * @param pid 进程 ID
     * @return 进程名称（如 "notepad.exe"），失败返回空字符串
     */
    std::string getProcessName(DWORD pid);

    /**
     * 映射优先级枚举到 Win32 常量
     * 
     * @param priority ProcessPriority 枚举值
     * @return Win32 优先级类常量（如 NORMAL_PRIORITY_CLASS）
     */
    DWORD mapPriorityToWin32(ProcessPriority priority);

    /**
     * 映射 Win32 常量到优先级枚举
     * 
     * @param win32Priority Win32 优先级类常量
     * @return ProcessPriority 枚举值
     */
    ProcessPriority mapWin32ToPriority(DWORD win32Priority);

    /**
     * 受保护的系统进程列表（硬编码）
     * 
     * 这些进程不允许被终止，以保护系统稳定性
     */
    static const std::vector<std::string> PROTECTED_PROCESSES;

    ConfigManager* configManager_;
    PolicyGuard* policyGuard_;
};

#endif // PROCESS_SERVICE_H
