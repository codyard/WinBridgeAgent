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
#include "support/dashboard_window.h"
#include "support/localization_manager.h"
#include <windowsx.h>
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <chrono>

#pragma comment(lib, "comctl32.lib")

namespace clawdesk {

// 控件 ID
#define ID_LOG_EDIT 2001
#define ID_CLEAR_BUTTON 2002
#define ID_COPY_BUTTON 2003
#define ID_STATUS_LABEL 2004
#define ID_COUNTDOWN_LABEL 2005
#define ID_CANCEL_SHUTDOWN_BUTTON 2006
#define ID_HIGH_RISK_COUNTER_LABEL 2007

// 自定义消息
#define WM_UPDATE_LOG_DISPLAY (WM_USER + 1)

// 静态成员初始化
DashboardWindow* DashboardWindow::instance_ = nullptr;

namespace {
std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, NULL, 0);
    if (size <= 0) {
        size = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, NULL, 0);
        if (size <= 0) {
            return L"";
        }
        std::wstring out(size - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, &out[0], size);
        return out;
    }
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &out[0], size);
    return out;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) {
        return "";
    }
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &out[0], size, NULL, NULL);
    return out;
}
} // namespace

DashboardWindow::DashboardWindow() 
    : hwnd_(NULL), logEdit_(NULL), clearButton_(NULL), 
      copyButton_(NULL), statusLabel_(NULL), 
      countdownLabel_(NULL), cancelShutdownButton_(NULL), 
      highRiskCounterLabel_(NULL), forceClose_(false),
      localizationManager_(nullptr) {
    instance_ = this;
    
    // Initialize state
    state_.shutdownScheduled = false;
    state_.shutdownAction = "";
    state_.shutdownDelay = 0;
    state_.highRiskOperationCount = 0;
}

DashboardWindow::~DashboardWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
    instance_ = nullptr;
}

void DashboardWindow::registerWindowClass() {
    // 检查是否已经注册
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    
    if (GetClassInfoExW(GetModuleHandleW(NULL), L"ClawDeskDashboardClass", &wc)) {
        return; // 已经注册
    }
    
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ClawDeskDashboardClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        // 注册失败，记录错误
        DWORD error = GetLastError();
        char errorMsg[256];
        sprintf(errorMsg, "Failed to register window class. Error: %lu", error);
        MessageBoxA(NULL, errorMsg, "Dashboard Error", MB_OK | MB_ICONERROR);
    }
}

void DashboardWindow::create() {
    if (hwnd_) {
        show();
        return;
    }

    registerWindowClass();

    // 创建窗口（置顶）
    std::wstring title = localizationManager_
        ? localizationManager_->getString("dashboard.title")
        : L"ClawDesk MCP Server - Dashboard";
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,  // 置顶 + 工具窗口样式
        L"ClawDeskDashboardClass",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        100, 100,  // 明确指定位置而不是 CW_USEDEFAULT
        800, 600,
        NULL, NULL,
        GetModuleHandleW(NULL),
        this  // 传递 this 指针
    );

    if (!hwnd_) {
        DWORD error = GetLastError();
        char errorMsg[256];
        sprintf(errorMsg, "Failed to create Dashboard window. Error: %lu", error);
        MessageBoxA(NULL, errorMsg, "Dashboard Error", MB_OK | MB_ICONERROR);
        return;
    }

    createControls();
    refreshLocalization();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);  // 确保窗口在前台
}

void DashboardWindow::createControls() {
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    
    // 倒计时横幅（初始隐藏）
    countdownLabel_ = CreateWindowExW(
        0, L"STATIC",
        L"",
        WS_CHILD | SS_CENTER,
        10, 10, 660, 30,
        hwnd_, (HMENU)ID_COUNTDOWN_LABEL,
        hInstance, NULL
    );
    
    // 取消关机按钮（初始隐藏）
    cancelShutdownButton_ = CreateWindowExW(
        0, L"BUTTON",
        L"",
        WS_CHILD | BS_PUSHBUTTON,
        680, 10, 90, 30,
        hwnd_, (HMENU)ID_CANCEL_SHUTDOWN_BUTTON,
        hInstance, NULL
    );
    
    // 状态标签
    statusLabel_ = CreateWindowExW(
        0, L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 50, 600, 20,
        hwnd_, (HMENU)ID_STATUS_LABEL,
        hInstance, NULL
    );
    
    // 高风险操作计数器（右侧）
    highRiskCounterLabel_ = CreateWindowExW(
        0, L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        620, 50, 150, 20,
        hwnd_, (HMENU)ID_HIGH_RISK_COUNTER_LABEL,
        hInstance, NULL
    );

    // 日志文本框（多行、只读、自动滚动）
    logEdit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | 
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 80, 760, 440,
        hwnd_, (HMENU)ID_LOG_EDIT,
        hInstance, NULL
    );

    // 设置等宽字体
    HFONT hFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );
    SendMessage(logEdit_, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 清空按钮
    clearButton_ = CreateWindowExW(
        0, L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 530, 120, 30,
        hwnd_, (HMENU)ID_CLEAR_BUTTON,
        hInstance, NULL
    );

    // 复制按钮
    copyButton_ = CreateWindowExW(
        0, L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        140, 530, 120, 30,
        hwnd_, (HMENU)ID_COPY_BUTTON,
        hInstance, NULL
    );
}

void DashboardWindow::hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void DashboardWindow::show() {
    if (!hwnd_) {
        // 窗口还未创建，先创建
        create();
        return;
    }
    
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
        // 显示时更新日志显示
        updateLogDisplay();
    }
}

bool DashboardWindow::isVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

std::string DashboardWindow::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_now;
    localtime_s(&tm_now, &time_t_now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void DashboardWindow::addLog(const DashboardLogEntry& entry) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    // 限制日志数量
    if (logs_.size() >= MAX_LOGS) {
        logs_.pop_front();
    }
    
    logs_.push_back(entry);
    
    // 使用 PostMessage 通知 UI 线程更新显示（线程安全）
    if (hwnd_ && IsWindowVisible(hwnd_)) {
        PostMessage(hwnd_, WM_UPDATE_LOG_DISPLAY, 0, 0);
    }
}

void DashboardWindow::logRequest(const std::string& tool, const std::string& params) {
    DashboardLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.type = "request";
    entry.tool = tool;
    entry.message = "Received request";
    entry.details = params;
    entry.highRisk = false;
    addLog(entry);
}

void DashboardWindow::logProcessing(const std::string& tool, const std::string& message) {
    DashboardLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.type = "processing";
    entry.tool = tool;
    entry.message = message;
    entry.highRisk = false;
    addLog(entry);
}

void DashboardWindow::logSuccess(const std::string& tool, const std::string& result) {
    DashboardLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.type = "success";
    entry.tool = tool;
    entry.message = "Completed successfully";
    entry.details = result;
    entry.highRisk = false;
    addLog(entry);
}

void DashboardWindow::logError(const std::string& tool, const std::string& error) {
    DashboardLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.type = "error";
    entry.tool = tool;
    entry.message = "Error occurred";
    entry.details = error;
    entry.highRisk = false;
    addLog(entry);
}

void DashboardWindow::logHighRiskOperation(const std::string& tool, const std::string& details) {
    DashboardLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.type = "high_risk";
    entry.tool = tool;
    entry.message = "HIGH RISK OPERATION";
    entry.details = details;
    entry.highRisk = true;
    addLog(entry);
    
    // 增加高风险操作计数
    state_.highRiskOperationCount++;
    updateHighRiskCounter(state_.highRiskOperationCount);
}

std::wstring DashboardWindow::formatLogEntry(const DashboardLogEntry& entry) {
    std::wostringstream oss;
    
    // 类型标记
    std::string typeMarker;
    if (entry.type == "request") {
        typeMarker = "[REQ]";
    } else if (entry.type == "processing") {
        typeMarker = "[PRO]";
    } else if (entry.type == "success") {
        typeMarker = "[OK ]";
    } else if (entry.type == "error") {
        typeMarker = "[ERR]";
    } else if (entry.type == "high_risk") {
        typeMarker = "[!!!]";  // 高风险操作标记
    } else {
        typeMarker = "[???]";
    }
    
    // 格式: [时间] [类型] 工具名 - 消息
    try {
        oss << L"[" << Utf8ToWide(entry.timestamp) << L"] "
            << Utf8ToWide(typeMarker) << L" "
            << Utf8ToWide(entry.tool) << L" - "
            << Utf8ToWide(entry.message);
        
        // 如果有详细信息，添加到下一行
        if (!entry.details.empty()) {
            oss << L"\n      " << Utf8ToWide(entry.details);
        }
    } catch (...) {
        return L"[ERROR] Failed to format log entry";
    }
    
    return oss.str();
}

void DashboardWindow::updateLogDisplay() {
    if (!logEdit_) {
        return;
    }
    
    // 复制日志数据以避免长时间持有锁
    std::deque<DashboardLogEntry> logsCopy;
    size_t logCount = 0;
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        logsCopy = logs_;
        logCount = logs_.size();
    }
    
    std::wostringstream oss;
    
    // 构建完整日志文本（使用复制的数据，不需要锁）
    try {
        for (size_t i = 0; i < logsCopy.size(); ++i) {
            oss << formatLogEntry(logsCopy[i]) << L"\r\n";
        }
    } catch (...) {
        return;
    }
    
    std::wstring logText = oss.str();
    
    // 更新文本框内容
    SetWindowTextW(logEdit_, logText.c_str());
    
    // 滚动到底部
    int textLength = GetWindowTextLengthW(logEdit_);
    SendMessage(logEdit_, EM_SETSEL, textLength, textLength);
    SendMessage(logEdit_, EM_SCROLLCARET, 0, 0);
    
    // 更新状态标签（使用之前复制的 logCount）
    if (statusLabel_) {
        if (localizationManager_) {
            std::wstring statusText = localizationManager_->getFormattedString(
                "dashboard.total_logs",
                {std::to_wstring((int)logCount), std::to_wstring(MAX_LOGS)});
            SetWindowTextW(statusLabel_, statusText.c_str());
        } else {
            wchar_t statusText[256];
            swprintf(statusText, 256, L"Total logs: %d (max %d)",
                     (int)logCount, MAX_LOGS);
            SetWindowTextW(statusLabel_, statusText);
        }
    }
}

void DashboardWindow::clearLogs() {
    std::lock_guard<std::mutex> lock(logMutex_);
    logs_.clear();
    
    if (logEdit_) {
        SetWindowTextW(logEdit_, L"");
    }
    
    if (statusLabel_) {
        std::wstring statusText = localizationManager_
            ? localizationManager_->getString("dashboard.logs_cleared")
            : L"Logs cleared";
        SetWindowTextW(statusLabel_, statusText.c_str());
    }
}

LRESULT CALLBACK DashboardWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DashboardWindow* window = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        window = (DashboardWindow*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    } else {
        window = (DashboardWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (window) {
        return window->handleMessage(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT DashboardWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_UPDATE_LOG_DISPLAY:
            // 在 UI 线程中更新日志显示
            updateLogDisplay();
            return 0;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_CLEAR_BUTTON) {
                clearLogs();
                return 0;
            } else if (LOWORD(wParam) == ID_COPY_BUTTON) {
                // 复制所有日志到剪贴板
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    
                    int textLength = GetWindowTextLengthW(logEdit_);
                    if (textLength > 0) {
                        wchar_t* buffer = new wchar_t[textLength + 1];
                        GetWindowTextW(logEdit_, buffer, textLength + 1);
                        
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (textLength + 1) * sizeof(wchar_t));
                        if (hMem) {
                            wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
                            if (pMem) {
                                wcscpy(pMem, buffer);
                                GlobalUnlock(hMem);
                                SetClipboardData(CF_UNICODETEXT, hMem);
                            } else {
                                GlobalFree(hMem);
                            }
                        }
                        
                        delete[] buffer;
                    }
                    
                    CloseClipboard();
                    std::wstring msg = localizationManager_
                        ? localizationManager_->getString("dashboard.logs_copied")
                        : L"Logs copied to clipboard";
                    std::wstring title = localizationManager_
                        ? localizationManager_->getString("dashboard.copy_success_title")
                        : L"Success";
                    MessageBoxW(hwnd, msg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            } else if (LOWORD(wParam) == ID_CANCEL_SHUTDOWN_BUTTON) {
                handleCancelShutdown();
                return 0;
            }
            break;
            
        case WM_CLOSE:
            // 如果是强制关闭，则销毁窗口
            if (forceClose_) {
                DestroyWindow(hwnd);
            } else {
                // 否则只是隐藏窗口
                hide();
            }
            return 0;
            
        case WM_DESTROY:
            hwnd_ = NULL;
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DashboardWindow::showShutdownCountdown(const std::string& action, int remainingSeconds) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    state_.shutdownScheduled = true;
    state_.shutdownAction = action;
    state_.shutdownDelay = remainingSeconds;
    state_.shutdownTime = std::chrono::system_clock::now() + 
                          std::chrono::seconds(remainingSeconds);
    
    if (countdownLabel_ && cancelShutdownButton_) {
        // 显示倒计时横幅和取消按钮
        ShowWindow(countdownLabel_, SW_SHOW);
        ShowWindow(cancelShutdownButton_, SW_SHOW);
        
        // 设置红色背景（通过重绘实现）
        updateCountdownDisplay();
    }
}

void DashboardWindow::hideShutdownCountdown() {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    state_.shutdownScheduled = false;
    state_.shutdownAction = "";
    state_.shutdownDelay = 0;
    
    if (countdownLabel_ && cancelShutdownButton_) {
        ShowWindow(countdownLabel_, SW_HIDE);
        ShowWindow(cancelShutdownButton_, SW_HIDE);
    }
}

void DashboardWindow::updateHighRiskCounter(int count) {
    state_.highRiskOperationCount = count;
    
    if (highRiskCounterLabel_) {
        if (localizationManager_) {
            std::wstring counterText = localizationManager_->getFormattedString(
                "dashboard.high_risk_counter",
                {std::to_wstring(count)});
            SetWindowTextW(highRiskCounterLabel_, counterText.c_str());
        } else {
            wchar_t counterText[128];
            swprintf(counterText, 128, L"High-Risk Ops: %d", count);
            SetWindowTextW(highRiskCounterLabel_, counterText);
        }
    }
}

void DashboardWindow::updateCountdownDisplay() {
    if (!state_.shutdownScheduled || !countdownLabel_) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
        state_.shutdownTime - now).count();
    
    if (remaining < 0) {
        remaining = 0;
    }
    
    if (countdownLabel_) {
        if (localizationManager_) {
            std::wstring text = localizationManager_->getFormattedString(
                "dashboard.countdown_warning",
                {Utf8ToWide(state_.shutdownAction), std::to_wstring((int)remaining)});
            SetWindowTextW(countdownLabel_, text.c_str());
        } else {
            wchar_t countdownText[256];
            swprintf(countdownText, 256, L"WARNING: System %s scheduled in %d seconds",
                     Utf8ToWide(state_.shutdownAction).c_str(), (int)remaining);
            SetWindowTextW(countdownLabel_, countdownText);
        }
    }
}

void DashboardWindow::handleCancelShutdown() {
    // 这里需要调用 PowerService 的 abortShutdown 方法
    // 由于 Dashboard 不直接依赖 PowerService，这个功能需要通过回调或事件实现
    // 暂时只是隐藏倒计时，实际取消操作需要在集成时实现
    
    hideShutdownCountdown();
    
    // 记录取消操作
    DashboardLogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.type = "processing";
    entry.tool = "abort_shutdown";
    entry.message = "User cancelled shutdown from Dashboard";
    entry.highRisk = false;
    addLog(entry);
    
    std::wstring msg = localizationManager_
        ? localizationManager_->getString("dashboard.cancel_shutdown_message")
        : L"Shutdown cancellation requested.\nPlease use abort_shutdown tool to confirm.";
    std::wstring title = localizationManager_
        ? localizationManager_->getString("dashboard.cancel_shutdown_title")
        : L"Cancel Shutdown";
    MessageBoxW(hwnd_, msg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

void DashboardWindow::setLocalizationManager(LocalizationManager* localizationManager) {
    localizationManager_ = localizationManager;
    refreshLocalization();
}

void DashboardWindow::refreshLocalization() {
    if (!hwnd_) {
        return;
    }

    if (localizationManager_) {
        std::wstring title = localizationManager_->getString("dashboard.title");
        SetWindowTextW(hwnd_, title.c_str());
    }

    if (clearButton_) {
        std::wstring text = localizationManager_
            ? localizationManager_->getString("dashboard.clear_logs")
            : L"Clear Logs";
        SetWindowTextW(clearButton_, text.c_str());
    }
    if (copyButton_) {
        std::wstring text = localizationManager_
            ? localizationManager_->getString("dashboard.copy_all")
            : L"Copy All";
        SetWindowTextW(copyButton_, text.c_str());
    }
    if (cancelShutdownButton_) {
        std::wstring text = localizationManager_
            ? localizationManager_->getString("dashboard.cancel_shutdown")
            : L"Cancel";
        SetWindowTextW(cancelShutdownButton_, text.c_str());
    }
    if (statusLabel_) {
        if (logs_.empty()) {
            std::wstring text = localizationManager_
                ? localizationManager_->getString("dashboard.status.default")
                : L"Real-time monitoring - Logs are displayed below";
            SetWindowTextW(statusLabel_, text.c_str());
        }
    }
    updateHighRiskCounter(state_.highRiskOperationCount);
    updateCountdownDisplay();
    if (!logs_.empty()) {
        updateLogDisplay();
    }
}

} // namespace clawdesk
