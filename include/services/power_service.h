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
#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include <string>
#include <windows.h>

// Forward declarations
class ConfigManager;
class PolicyGuard;

/**
 * PowerAction - 电源操作类型枚举
 * 
 * 定义支持的电源管理操作：
 * - Shutdown: 关机
 * - Reboot: 重启
 * - Hibernate: 休眠
 * - Sleep: 睡眠
 */
enum class PowerAction {
    Shutdown,
    Reboot,
    Hibernate,
    Sleep
};

/**
 * ShutdownResult - 关机/重启操作结果
 * 
 * 包含电源操作的结果信息：
 * - success: 操作是否成功
 * - action: 执行的操作类型
 * - delay: 延迟时间（秒）
 * - scheduledTime: 计划执行时间（ISO 8601 格式）
 * - error: 错误信息（如果失败）
 */
struct ShutdownResult {
    bool success;
    PowerAction action;
    int delay;
    std::string scheduledTime;
    std::string error;
};

/**
 * AbortShutdownResult - 取消关机操作结果
 * 
 * 包含取消关机操作的结果信息：
 * - success: 操作是否成功
 * - message: 操作消息
 * - error: 错误信息（如果失败）
 */
struct AbortShutdownResult {
    bool success;
    std::string message;
    std::string error;
};

/**
 * PowerService - 电源管理服务
 * 
 * 提供系统关机、重启、休眠、睡眠功能。
 * 
 * 功能：
 * - 关机（带延迟）
 * - 重启（带延迟）
 * - 休眠
 * - 睡眠
 * - 取消计划的关机/重启
 * - SE_SHUTDOWN_NAME 权限启用
 * 
 * 需求覆盖：
 * - 30.1: 执行指定的电源操作
 * - 30.2: 支持 shutdown、reboot、hibernate、sleep 操作
 * - 30.3: 支持延迟执行（delay 参数）
 * - 30.4: 支持强制关闭应用程序（force 参数）
 * - 30.5: 支持显示消息（message 参数）
 * - 30.6: 需要 SE_SHUTDOWN_NAME 权限
 * - 30.7: 记录操作到审计日志
 * - 31.1: 取消计划的关机或重启
 * - 31.2: 需要 SE_SHUTDOWN_NAME 权限
 * - 31.3: 返回成功状态
 * - 31.4: 返回错误信息
 */
class PowerService {
public:
    /**
     * 构造函数
     * @param configManager 配置管理器指针
     * @param policyGuard 策略守卫指针（可选，可为 nullptr）
     */
    PowerService(ConfigManager* configManager, PolicyGuard* policyGuard);

    /**
     * 执行电源操作
     * 
     * @param action 电源操作类型（Shutdown/Reboot/Hibernate/Sleep）
     * @param delay 延迟时间（秒），默认 0（立即执行）
     * @param force 是否强制关闭应用程序，默认 false
     * @param message 显示给用户的消息，默认为空
     * @return ShutdownResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 启用 SE_SHUTDOWN_NAME 权限
     * 2. 根据 action 类型调用相应的 Win32 API：
     *    - Shutdown/Reboot: InitiateSystemShutdownEx
     *    - Hibernate/Sleep: SetSuspendState
     * 3. 计算并格式化计划时间（当前时间 + delay）
     * 4. 返回结果
     */
    ShutdownResult shutdownSystem(PowerAction action,
                                  int delay = 0,
                                  bool force = false,
                                  const std::string& message = "");

    /**
     * 取消计划的关机/重启
     * 
     * @return AbortShutdownResult 包含操作结果
     * 
     * 实现逻辑：
     * 1. 启用 SE_SHUTDOWN_NAME 权限
     * 2. 调用 AbortSystemShutdown(NULL)
     * 3. 返回结果
     */
    AbortShutdownResult abortShutdown();

private:
    /**
     * 启用关机权限
     * 
     * 使用 AdjustTokenPrivileges 启用 SE_SHUTDOWN_NAME 权限
     * 
     * @return true 表示权限启用成功
     */
    bool enableShutdownPrivilege();

    /**
     * 执行关机/重启
     * 
     * 使用 InitiateSystemShutdownEx API 执行关机或重启
     * 
     * @param action 电源操作类型（Shutdown 或 Reboot）
     * @param delay 延迟时间（秒）
     * @param force 是否强制关闭应用程序
     * @param message 显示给用户的消息
     * @return true 表示操作成功
     */
    bool initiateShutdown(PowerAction action, int delay, bool force, const std::string& message);

    /**
     * 执行休眠/睡眠
     * 
     * 使用 SetSuspendState API 执行休眠或睡眠
     * 
     * @param action 电源操作类型（Hibernate 或 Sleep）
     * @return true 表示操作成功
     */
    bool initiateSuspend(PowerAction action);

    /**
     * 格式化计划时间
     * 
     * 计算当前时间 + delay 秒，并格式化为 ISO 8601 格式
     * 
     * @param delay 延迟时间（秒）
     * @return ISO 8601 格式的时间字符串（如 "2026-02-03T12:11:00Z"）
     */
    std::string formatScheduledTime(int delay);

    ConfigManager* configManager_;
    PolicyGuard* policyGuard_;
};

#endif // POWER_SERVICE_H
