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
#include "support/download_progress_window.h"
#include "support/localization_manager.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <memory>

#pragma comment(lib, "comctl32.lib")

namespace clawdesk {

DownloadProgressWindow::DownloadProgressWindow()
    : hwnd_(nullptr)
    , progress_bar_(nullptr)
    , status_label_(nullptr)
    , speed_label_(nullptr)
    , cancel_button_(nullptr)
    , window_thread_(nullptr)
    , window_thread_id_(0)
    , progress_percentage_(0)
    , is_visible_(false)
    , title_(L"Downloading Update")
{
}

DownloadProgressWindow::~DownloadProgressWindow() {
    close();
}

bool DownloadProgressWindow::create(HWND parent_window) {
    if (hwnd_) {
        return true;
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"ClawDeskDownloadProgressWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"ClawDeskDownloadProgressWindow",
        title_.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        parent_window,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!hwnd_) {
        return false;
    }

    createControls();
    centerWindow();
    
    return true;
}

void DownloadProgressWindow::createControls() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    
    int y_pos = CONTROL_MARGIN;
    
    status_label_ = CreateWindowExW(
        0,
        L"STATIC",
        L"Preparing download...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CONTROL_MARGIN,
        y_pos,
        WINDOW_WIDTH - CONTROL_MARGIN * 2,
        20,
        hwnd_,
        nullptr,
        hInstance,
        nullptr
    );
    
    y_pos += 30;
    
    progress_bar_ = CreateWindowExW(
        0,
        PROGRESS_CLASSW,
        nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        CONTROL_MARGIN,
        y_pos,
        WINDOW_WIDTH - CONTROL_MARGIN * 2,
        25,
        hwnd_,
        nullptr,
        hInstance,
        nullptr
    );
    
    SendMessage(progress_bar_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(progress_bar_, PBM_SETPOS, 0, 0);
    
    y_pos += 35;
    
    speed_label_ = CreateWindowExW(
        0,
        L"STATIC",
        L"Speed: 0 MB/s",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CONTROL_MARGIN,
        y_pos,
        WINDOW_WIDTH - CONTROL_MARGIN * 2,
        20,
        hwnd_,
        nullptr,
        hInstance,
        nullptr
    );
    
    y_pos += 30;
    
    cancel_button_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        (WINDOW_WIDTH - 100) / 2,
        y_pos,
        100,
        30,
        hwnd_,
        (HMENU)1,
        hInstance,
        nullptr
    );
    
    HFONT hFont = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
    
    SendMessage(status_label_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(speed_label_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(cancel_button_, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void DownloadProgressWindow::show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        is_visible_ = true;
    }
}

void DownloadProgressWindow::hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        is_visible_ = false;
    }
}

void DownloadProgressWindow::close() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        is_visible_ = false;
    }
}

void DownloadProgressWindow::updateProgress(const DownloadProgress& progress) {
    if (!hwnd_) {
        return;
    }

    // 分配进度数据副本（UI 线程负责释放）
    DownloadProgress* progressCopy = new DownloadProgress(progress);
    
    // 使用 PostMessage 将进度数据发送到 UI 线程
    // 这样可以从任何线程安全地调用此方法
    PostMessage(hwnd_, WM_DOWNLOAD_PROGRESS_UPDATE, 0, reinterpret_cast<LPARAM>(progressCopy));
}

void DownloadProgressWindow::setTitle(const std::string& title) {
    title_ = std::wstring(title.begin(), title.end());
    if (hwnd_) {
        SetWindowTextW(hwnd_, title_.c_str());
    }
}

void DownloadProgressWindow::setCancelCallback(std::function<void()> callback) {
    cancel_callback_ = callback;
}

bool DownloadProgressWindow::isVisible() const {
    return is_visible_;
}

void DownloadProgressWindow::centerWindow() {
    if (!hwnd_) {
        return;
    }

    RECT rect;
    GetWindowRect(hwnd_, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    int x = (screen_width - width) / 2;
    int y = (screen_height - height) / 2;

    SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

LRESULT CALLBACK DownloadProgressWindow::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DownloadProgressWindow* window = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<DownloadProgressWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<DownloadProgressWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_DOWNLOAD_PROGRESS_UPDATE:
            // 在 UI 线程中处理进度更新
            if (window) {
                DownloadProgress* progress = reinterpret_cast<DownloadProgress*>(lParam);
                if (progress) {
                    // 使用智能指针自动释放内存
                    std::unique_ptr<DownloadProgress> progressPtr(progress);
                    
                    // 更新 UI（现在是线程安全的）
                    window->progress_percentage_ = progress->percentage;
                    
                    std::wstring status_str(progress->status_message.begin(), progress->status_message.end());
                    window->status_text_ = status_str;
                    
                    std::wostringstream speed_oss;
                    double speed_mb = progress->speed_bytes_per_sec / 1024.0 / 1024.0;
                    speed_oss << L"Speed: " << std::fixed << std::setprecision(2) << speed_mb << L" MB/s";
                    window->speed_text_ = speed_oss.str();
                    
                    SendMessage(window->progress_bar_, PBM_SETPOS, window->progress_percentage_, 0);
                    SetWindowTextW(window->status_label_, window->status_text_.c_str());
                    SetWindowTextW(window->speed_label_, window->speed_text_.c_str());
                    
                    if (progress->is_complete) {
                        EnableWindow(window->cancel_button_, FALSE);
                        SetWindowTextW(window->cancel_button_, L"Close");
                    }
                }
            }
            return 0;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) {
                if (window && window->cancel_callback_) {
                    window->cancel_callback_();
                }
                if (window) {
                    window->close();
                }
            }
            return 0;

        case WM_CLOSE:
            if (window && window->cancel_callback_) {
                window->cancel_callback_();
            }
            if (window) {
                window->close();
            }
            return 0;

        case WM_DESTROY:
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace clawdesk
