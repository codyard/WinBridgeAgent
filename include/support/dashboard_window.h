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
#ifndef CLAWDESK_DASHBOARD_WINDOW_H
#define CLAWDESK_DASHBOARD_WINDOW_H

#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <chrono>

class LocalizationManager;  // Forward declaration outside namespace

namespace clawdesk {

// Dashboard log entry
struct DashboardLogEntry {
    std::string timestamp;      // 时间戳
    std::string type;           // 类型: "request", "processing", "success", "error"
    std::string tool;           // 工具名称
    std::string message;        // 消息内容
    std::string details;        // 详细信息（可选）
    bool highRisk;              // v0.3.0: 是否为高风险操作
};

// Dashboard state for shutdown countdown
struct DashboardState {
    bool shutdownScheduled;
    std::string shutdownAction;
    int shutdownDelay;
    std::chrono::system_clock::time_point shutdownTime;
    int highRiskOperationCount;
};

/**
 * DashboardWindow - 实时监控窗口
 * 
 * 功能：
 * - 显示实时接收的指令
 * - 显示处理过程和结果
 * - 窗口置顶显示
 * - 自动滚动到最新日志
 * - 支持清空日志
 * - 支持复制日志内容
 */
class DashboardWindow {
public:
    DashboardWindow();
    ~DashboardWindow();

    // 创建并显示窗口
    void create();

    // 隐藏窗口
    void hide();

    // 显示窗口
    void show();

    // 检查窗口是否可见
    bool isVisible() const;

    // 添加日志条目
    void addLog(const DashboardLogEntry& entry);

    // 添加请求日志
    void logRequest(const std::string& tool, const std::string& params);

    // 添加处理中日志
    void logProcessing(const std::string& tool, const std::string& message);

    // 添加成功日志
    void logSuccess(const std::string& tool, const std::string& result);

    // 添加错误日志
    void logError(const std::string& tool, const std::string& error);

    // v0.3.0: 添加高风险操作日志
    void logHighRiskOperation(const std::string& tool, const std::string& details);

    // v0.3.0: 显示关机倒计时
    void showShutdownCountdown(const std::string& action, int remainingSeconds);

    // v0.3.0: 隐藏关机倒计时
    void hideShutdownCountdown();

    // v0.3.0: 更新高风险操作计数器
    void updateHighRiskCounter(int count);

    // 清空日志
    void clearLogs();

    // 设置本地化管理器
    void setLocalizationManager(::LocalizationManager* localizationManager);

    // 刷新本地化文本
    void refreshLocalization();

    // 设置强制关闭标志（用于程序退出）
    void setForceClose(bool force) { forceClose_ = force; }

    // 获取窗口句柄
    HWND getHandle() const { return hwnd_; }

private:
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 实例窗口过程
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 注册窗口类
    void registerWindowClass();

    // 创建窗口控件
    void createControls();

    // 更新日志显示
    void updateLogDisplay();

    // 格式化日志条目为文本
    std::wstring formatLogEntry(const DashboardLogEntry& entry);

    // 获取当前时间戳
    std::string getCurrentTimestamp();

    // 获取类型颜色（用于显示）
    COLORREF getTypeColor(const std::string& type);

    // v0.3.0: 渲染关机倒计时横幅
    void renderShutdownCountdown();

    // v0.3.0: 更新关机倒计时显示
    void updateCountdownDisplay();

    // v0.3.0: 处理取消关机按钮
    void handleCancelShutdown();

    HWND hwnd_;                             // 窗口句柄
    HWND logEdit_;                          // 日志文本框句柄
    HWND clearButton_;                      // 清空按钮句柄
    HWND copyButton_;                       // 复制按钮句柄
    HWND statusLabel_;                      // 状态标签句柄
    HWND countdownLabel_;                   // v0.3.0: 倒计时标签句柄
    HWND cancelShutdownButton_;             // v0.3.0: 取消关机按钮句柄
    HWND highRiskCounterLabel_;             // v0.3.0: 高风险操作计数器标签
    
    std::deque<DashboardLogEntry> logs_;    // 日志队列（最多保留 1000 条）
    std::mutex logMutex_;                   // 日志访问锁
    bool forceClose_;                       // 强制关闭标志
    DashboardState state_;                  // v0.3.0: Dashboard 状态
    ::LocalizationManager* localizationManager_;
    
    static DashboardWindow* instance_;      // 单例实例
    static const int MAX_LOGS = 1000;       // 最大日志条数
};

} // namespace clawdesk

#endif // CLAWDESK_DASHBOARD_WINDOW_H
