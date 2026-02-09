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
#ifndef DOWNLOAD_PROGRESS_WINDOW_H
#define DOWNLOAD_PROGRESS_WINDOW_H

#include <windows.h>
#include <string>
#include <functional>
#include "support/download_manager.h"

// 自定义消息用于线程安全的进度更新
#define WM_DOWNLOAD_PROGRESS_UPDATE (WM_USER + 100)

namespace clawdesk {

class DownloadProgressWindow {
public:
    DownloadProgressWindow();
    ~DownloadProgressWindow();

    bool create(HWND parent_window = nullptr);
    void show();
    void hide();
    void close();
    
    // 线程安全的进度更新（可从任何线程调用）
    void updateProgress(const DownloadProgress& progress);
    void setTitle(const std::string& title);
    void setCancelCallback(std::function<void()> callback);
    
    bool isVisible() const;
    HWND getHandle() const { return hwnd_; }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI windowThreadProc(LPVOID param);
    
    void createControls();
    void updateUI();
    void centerWindow();
    
    HWND hwnd_;
    HWND progress_bar_;
    HWND status_label_;
    HWND speed_label_;
    HWND cancel_button_;
    
    HANDLE window_thread_;
    DWORD window_thread_id_;
    
    std::function<void()> cancel_callback_;
    
    std::wstring title_;
    std::wstring status_text_;
    std::wstring speed_text_;
    int progress_percentage_;
    bool is_visible_;
    
    static const int WINDOW_WIDTH = 500;
    static const int WINDOW_HEIGHT = 200;
    static const int CONTROL_MARGIN = 20;
};

} // namespace clawdesk

#endif // DOWNLOAD_PROGRESS_WINDOW_H
