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
// 确保使用 Unicode 版本的 Windows API
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "support/settings_window.h"
#include "support/settings_window_ids.h"
#include "support/config_manager.h"
#include "support/auto_startup_manager.h"
#include "support/localization_manager.h"
#include "support/license_manager.h"
#include "support/audit_logger.h"
#include "support/dashboard_window.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <sstream>
#include <random>
#include <map>
#include <cwctype>
#include <shlwapi.h>
#include <algorithm>
#include <cctype>

extern clawdesk::DashboardWindow* g_dashboard;
extern clawdesk::AuditLogger* g_auditLogger;
void UpdateTrayIconTooltip();

#ifndef CLAWDESK_VERSION
#define CLAWDESK_VERSION "0.3.0"
#endif

namespace {
// Forward declarations
std::wstring GetSelectedText(HWND list, int index);

const wchar_t* kDaemonMutexName = L"WinBridgeAgentDaemon_SingleInstance";
const wchar_t* kDaemonExitEvent = L"WinBridgeAgentDaemon_Exit";
const wchar_t* kTabPageProp = L"ClawDeskTabPageOwner";
const wchar_t* kTabPageProcProp = L"ClawDeskTabPageProc";

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

bool IsDaemonRunning() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, kDaemonMutexName);
    if (mutex) {
        CloseHandle(mutex);
        return true;
    }
    return false;
}

void SignalDaemonExit() {
    HANDLE evt = OpenEventW(EVENT_MODIFY_STATE, FALSE, kDaemonExitEvent);
    if (!evt) {
        evt = CreateEventW(NULL, TRUE, FALSE, kDaemonExitEvent);
    }
    if (evt) {
        SetEvent(evt);
        CloseHandle(evt);
    }
}

void StartDaemonIfNeeded() {
    if (IsDaemonRunning()) {
        return;
    }
    std::wstring dir = GetExeDirW();
    if (dir.empty()) {
        return;
    }
    std::wstring daemonPath = dir + L"\\WinBridgeAgentDaemon.exe";
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    if (CreateProcessW(daemonPath.c_str(), NULL, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

struct InputDialogState {
    std::wstring title;
    std::wstring label;
    std::wstring value;
    std::wstring defaultValue;
    bool accepted = false;
    HWND edit = nullptr;
    HWND parent = nullptr;
};

LRESULT CALLBACK TabPageSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                     UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_COMMAND || msg == WM_NOTIFY) {
        SettingsWindow* window = reinterpret_cast<SettingsWindow*>(dwRefData);
        if (window) {
            HWND mainHwnd = window->getHwnd();
            if (mainHwnd) {
                return SendMessage(mainHwnd, msg, wParam, lParam);
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TabPageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND || msg == WM_NOTIFY) {
        SettingsWindow* window = reinterpret_cast<SettingsWindow*>(GetPropW(hwnd, kTabPageProp));
        if (window) {
            HWND mainHwnd = window->getHwnd();
            if (mainHwnd) {
                return SendMessage(mainHwnd, msg, wParam, lParam);
            }
        }
    }
    WNDPROC orig = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kTabPageProcProp));
    if (orig) {
        return CallWindowProcW(orig, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void AttachTabPageHandler(HWND page, SettingsWindow* window) {
    if (!page) {
        return;
    }
    if (SetWindowSubclass(page, TabPageSubclassProc, 1, reinterpret_cast<DWORD_PTR>(window))) {
        return;
    }
    SetPropW(page, kTabPageProp, reinterpret_cast<HANDLE>(window));
    WNDPROC orig = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(page, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TabPageWndProc)));
    if (orig) {
        SetPropW(page, kTabPageProcProp, reinterpret_cast<HANDLE>(orig));
    }
}

LRESULT CALLBACK InputDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InputDialogState* state = reinterpret_cast<InputDialogState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        state = reinterpret_cast<InputDialogState*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        HINSTANCE hInst = GetModuleHandleW(NULL);

        CreateWindowExW(
            0,
            L"STATIC",
            state ? state->label.c_str() : L"Enter value:",
            WS_CHILD | WS_VISIBLE,
            12, 12, 260, 16,
            hwnd,
            (HMENU)1000,
            hInst,
            NULL);

        state->edit = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            12, 32, 260, 22,
            hwnd,
            (HMENU)1001,
            hInst,
            NULL);
        if (state && !state->defaultValue.empty()) {
            SetWindowTextW(state->edit, state->defaultValue.c_str());
            SendMessageW(state->edit, EM_SETSEL, 0, -1);
        }

        CreateWindowExW(
            0,
            L"BUTTON",
            L"OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            122, 64, 70, 22,
            hwnd,
            (HMENU)IDOK,
            hInst,
            NULL);

        CreateWindowExW(
            0,
            L"BUTTON",
            L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            202, 64, 70, 22,
            hwnd,
            (HMENU)IDCANCEL,
            hInst,
            NULL);

        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            if (state && state->edit) {
                wchar_t buffer[512];
                GetWindowTextW(state->edit, buffer, 512);
                state->value = buffer;
                state->accepted = true;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state && state->parent) {
            EnableWindow(state->parent, TRUE);
            SetForegroundWindow(state->parent);
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool PromptForText(HWND parent, const std::wstring& title, const std::wstring& label, std::wstring* inOutValue) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = InputDialogProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"ClawDeskInputDialog";
        RegisterClassExW(&wc);
        registered = true;
    }

    InputDialogState state;
    state.title = title;
    state.label = label;
    if (inOutValue) {
        state.defaultValue = *inOutValue;
    }
    state.parent = parent;

    EnableWindow(parent, FALSE);
    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"ClawDeskInputDialog",
        title.c_str(),
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        300, 130,
        parent,
        NULL,
        GetModuleHandleW(NULL),
        &state);

    if (!hwnd) {
        EnableWindow(parent, TRUE);
        return false;
    }

    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (state.accepted && inOutValue) {
        *inOutValue = state.value;
    }
    return state.accepted;
}

std::wstring GetEditText(HWND edit) {
    wchar_t buffer[4096];
    GetWindowTextW(edit, buffer, 4096);
    return buffer;
}

void SetEditText(HWND edit, const std::wstring& text) {
    SetWindowTextW(edit, text.c_str());
}

std::vector<std::wstring> GetListBoxItemsW(HWND list) {
    std::vector<std::wstring> items;
    if (!list) {
        return items;
    }
    LRESULT count = SendMessageW(list, LB_GETCOUNT, 0, 0);
    if (count == LB_ERR) {
        return items;
    }
    items.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        LRESULT len = SendMessageW(list, LB_GETTEXTLEN, i, 0);
        if (len == LB_ERR) {
            continue;
        }
        std::wstring buffer(static_cast<size_t>(len) + 1, L'\0');
        SendMessageW(list, LB_GETTEXT, i, (LPARAM)buffer.data());
        buffer.resize(wcslen(buffer.c_str()));
        items.push_back(buffer);
    }
    return items;
}

void SetListBoxItems(HWND list, const std::vector<std::string>& items) {
    if (!list) {
        return;
    }
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& item : items) {
        std::wstring w(item.begin(), item.end());
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)w.c_str());
    }
}

bool ListBoxContains(HWND list, const std::wstring& value, int ignoreIndex = -1) {
    if (!list) {
        return false;
    }
    LRESULT count = SendMessageW(list, LB_GETCOUNT, 0, 0);
    if (count == LB_ERR) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (i == ignoreIndex) {
            continue;
        }
        std::wstring entry = GetSelectedText(list, i);
        if (_wcsicmp(entry.c_str(), value.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

bool IsDirectoryPath(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsFilePath(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void SetListBoxItemHeight(HWND list, int height) {
    if (!list) {
        return;
    }
    SendMessageW(list, LB_SETITEMHEIGHT, 0, height);
}

std::wstring TrimString(const std::wstring& value) {
    if (value.empty()) {
        return value;
    }
    size_t start = 0;
    while (start < value.size() && iswspace(value[start])) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && iswspace(value[end - 1])) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string TrimStringAnsi(const std::string& value) {
    if (value.empty()) {
        return value;
    }
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::wstring FormatAppEntry(const std::wstring& path) {
    std::wstring filename = PathFindFileNameW(path.c_str());
    std::wstring name = filename;
    size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos) {
        name = name.substr(0, dot);
    }
    if (name.empty()) {
        name = filename;
    }
    return name + L" = " + path;
}

void SplitAppEntryW(const std::wstring& entry, std::wstring* nameOut, std::wstring* pathOut) {
    if (nameOut) {
        *nameOut = L"";
    }
    if (pathOut) {
        *pathOut = L"";
    }
    size_t pos = entry.find(L'=');
    if (pos == std::wstring::npos) {
        if (nameOut) {
            *nameOut = TrimString(entry);
        }
        return;
    }
    if (nameOut) {
        *nameOut = TrimString(entry.substr(0, pos));
    }
    if (pathOut) {
        *pathOut = TrimString(entry.substr(pos + 1));
    }
}

bool AppEntryConflicts(HWND list, const std::wstring& entry, int ignoreIndex = -1) {
    if (!list) {
        return false;
    }
    std::wstring name;
    std::wstring path;
    SplitAppEntryW(entry, &name, &path);
    LRESULT count = SendMessageW(list, LB_GETCOUNT, 0, 0);
    if (count == LB_ERR) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (i == ignoreIndex) {
            continue;
        }
        std::wstring existing = GetSelectedText(list, i);
        std::wstring existingName;
        std::wstring existingPath;
        SplitAppEntryW(existing, &existingName, &existingPath);
        if (!name.empty() && !_wcsicmp(existingName.c_str(), name.c_str())) {
            return true;
        }
        if (!path.empty() && !_wcsicmp(existingPath.c_str(), path.c_str())) {
            return true;
        }
    }
    return false;
}

void ShowValidationError(HWND hwnd, LocalizationManager* lm, const std::wstring& fallback, const char* key) {
    std::wstring text = fallback;
    if (lm && key) {
        text = lm->getString(key);
    }
    MessageBoxW(hwnd, text.c_str(), L"Settings", MB_OK | MB_ICONWARNING);
}

bool AddListItem(HWND list, const std::wstring& value, int insertAt = -1) {
    if (!list || value.empty()) {
        return false;
    }
    LRESULT index = 0;
    if (insertAt < 0) {
        index = SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)value.c_str());
    } else {
        index = SendMessageW(list, LB_INSERTSTRING, insertAt, (LPARAM)value.c_str());
    }
    if (index != LB_ERR) {
        SendMessageW(list, LB_SETCURSEL, index, 0);
    }
    return true;
}

int GetSelectedIndex(HWND list) {
    if (!list) {
        return LB_ERR;
    }
    return (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
}

std::wstring GetSelectedText(HWND list, int index) {
    if (!list || index == LB_ERR) {
        return L"";
    }
    LRESULT len = SendMessageW(list, LB_GETTEXTLEN, index, 0);
    if (len == LB_ERR) {
        return L"";
    }
    std::wstring buffer(static_cast<size_t>(len), L'\0');
    SendMessageW(list, LB_GETTEXT, index, (LPARAM)buffer.data());
    return buffer;
}

bool RemoveSelectedListItem(HWND list) {
    int index = GetSelectedIndex(list);
    if (index == LB_ERR) {
        return false;
    }
    SendMessageW(list, LB_DELETESTRING, index, 0);
    return true;
}

bool MoveSelectedListItem(HWND list, int delta) {
    int index = GetSelectedIndex(list);
    if (index == LB_ERR) {
        return false;
    }
    int target = index + delta;
    LRESULT count = SendMessageW(list, LB_GETCOUNT, 0, 0);
    if (count == LB_ERR || target < 0 || target >= count) {
        return false;
    }
    std::wstring value = GetSelectedText(list, index);
    SendMessageW(list, LB_DELETESTRING, index, 0);
    SendMessageW(list, LB_INSERTSTRING, target, (LPARAM)value.c_str());
    SendMessageW(list, LB_SETCURSEL, target, 0);
    return true;
}

void AppendLine(HWND edit, const std::wstring& value) {
    if (!edit || value.empty()) {
        return;
    }
    std::wstring text = GetEditText(edit);
    if (!text.empty() && text.rfind(L"\r\n") != text.size() - 2) {
        text += L"\r\n";
    }
    text += value;
    text += L"\r\n";
    SetEditText(edit, text);
}

void RemoveSelectedLine(HWND edit) {
    if (!edit) {
        return;
    }
    std::wstring text = GetEditText(edit);
    if (text.empty()) {
        return;
    }
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(edit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    size_t pos = static_cast<size_t>(start);
    size_t lineStart = text.rfind(L"\r\n", pos == 0 ? 0 : pos - 1);
    if (lineStart == std::wstring::npos) {
        lineStart = 0;
    } else {
        lineStart += 2;
    }
    size_t lineEnd = text.find(L"\r\n", pos);
    if (lineEnd == std::wstring::npos) {
        lineEnd = text.size();
    } else {
        lineEnd += 2;
    }
    if (lineStart >= text.size()) {
        return;
    }
    text.erase(lineStart, lineEnd - lineStart);
    if (text.rfind(L"\r\n", 0) == 0) {
        text.erase(0, 2);
    }
    if (text.size() >= 2 && text.rfind(L"\r\n") == text.size() - 2) {
        text.erase(text.size() - 2);
    }
    SetEditText(edit, text);
}
} // namespace

SettingsWindow::SettingsWindow(HINSTANCE hInstance,
                               HWND parentWindow,
                               ConfigManager* configManager,
                               LicenseManager* licenseManager,
                               clawdesk::AuditLogger* auditLogger,
                               LocalizationManager* localizationManager)
    : hInstance_(hInstance),
      parentWindow_(parentWindow),
      hwnd_(NULL),
      tabControl_(NULL),
      dialogResult_(false),
      hasUnsavedChanges_(false),
      configManager_(configManager),
      licenseManager_(licenseManager),
      auditLogger_(auditLogger),
      localizationManager_(localizationManager),
      uiFont_(NULL) {
}

bool SettingsWindow::Show() {
    dialogResult_ = false;
    
    // 注册窗口类
    registerWindowClass();
    
    // 创建主窗口（已经包含 WS_VISIBLE）
    createMainWindow();
    
    if (!hwnd_) {
        MessageBoxA(parentWindow_, 
                    "Failed to create Settings window.",
                    "Error", 
                    MB_OK | MB_ICONERROR);
        return false;
    }
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    return dialogResult_;
}

void SettingsWindow::registerWindowClass() {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ClawDeskSettingsWindow";
    
    RegisterClassExW(&wc);
}

void SettingsWindow::createMainWindow() {
    // 先加载设置
    LoadSettings();
    
    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"ClawDeskSettingsWindow",
        L"Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        520, 520,
        parentWindow_,
        NULL,
        hInstance_,
        this
    );
    
    if (hwnd_) {
        SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        
        // 初始化 Common Controls
        InitCommonControls();
        
        // 创建 Tab 控件
        tabControl_ = CreateWindowExW(
            0,
            WC_TABCONTROLW,
            NULL,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            8, 8, 504, 430,
            hwnd_,
            (HMENU)IDC_SETTINGS_TAB,
            hInstance_,
            NULL
        );
        
        // 创建按钮
        CreateWindowExW(
            0,
            L"BUTTON",
            L"OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            302, 450, 60, 24,
            hwnd_,
            (HMENU)IDOK,
            hInstance_,
            NULL
        );
        
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            368, 450, 60, 24,
            hwnd_,
            (HMENU)IDCANCEL,
            hInstance_,
            NULL
        );
        
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Apply",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            434, 450, 60, 24,
            hwnd_,
            (HMENU)IDC_BUTTON_APPLY,
            hInstance_,
            NULL
        );
        
        // 初始化标签页
        initializeTabs();
        
        // 更新标签页标题
        updateTabTitles();

        // Apply consistent UI font across controls
        applyUIFont();
    }
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* window = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (!window) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    switch (msg) {
    case WM_CLOSE:
        window->onCancelClicked();
        return 0;
        
    case WM_DESTROY:
        if (window->uiFont_) {
            DeleteObject(window->uiFont_);
            window->uiFont_ = NULL;
        }
        PostQuitMessage(0);
        return 0;
        
    case WM_COMMAND:
        return DialogProc(hwnd, msg, wParam, lParam);
        
    case WM_NOTIFY:
        return DialogProc(hwnd, msg, wParam, lParam);
        
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void SettingsWindow::Hide() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
    }
}

bool SettingsWindow::LoadSettings() {
    if (!configManager_) {
        return false;
    }
    currentSettings_.language = configManager_->getLanguage();
    currentSettings_.autoStartup = configManager_->isAutoStartupEnabled();
    currentSettings_.apiKey = configManager_->getApiKey();
    currentSettings_.apiKeyValid = false;
    currentSettings_.serverPort = configManager_->getServerPort();
    currentSettings_.autoPortSelection = configManager_->isAutoPortEnabled();
    currentSettings_.listenAddress = configManager_->getListenAddress();
    currentSettings_.bearerToken = configManager_->getAuthToken();
    currentSettings_.allowedDirectories = configManager_->getAllowedDirs();
    currentSettings_.allowedCommands = configManager_->getAllowedCommands();
    currentSettings_.allowedApplications.clear();
    for (const auto& pair : configManager_->getAllowedApps()) {
        if (pair.second.empty()) {
            currentSettings_.allowedApplications.push_back(pair.first);
        } else {
            currentSettings_.allowedApplications.push_back(pair.first + " = " + pair.second);
        }
    }
    currentSettings_.highRiskConfirmations = configManager_->isHighRiskConfirmationEnabled();
    currentSettings_.dashboardAutoShow = configManager_->isDashboardAutoShowEnabled();
    currentSettings_.dashboardAlwaysOnTop = configManager_->isDashboardAlwaysOnTopEnabled();
    currentSettings_.trayIconStyle = configManager_->getTrayIconStyle();
    currentSettings_.logRetentionDays = configManager_->getLogRetentionDays();
    currentSettings_.daemonEnabled = configManager_->isDaemonEnabled();

    originalSettings_ = currentSettings_;
    return true;
}

bool SettingsWindow::SaveSettings() {
    if (!configManager_) {
        return false;
    }
    try {
        configManager_->setLanguage(currentSettings_.language);
        configManager_->setAutoStartup(currentSettings_.autoStartup);
        configManager_->setApiKey(currentSettings_.apiKey);
        configManager_->setAuthToken(currentSettings_.bearerToken);
        configManager_->setAllowedDirs(currentSettings_.allowedDirectories);
        configManager_->setAllowedCommands(currentSettings_.allowedCommands);
        std::map<std::string, std::string> apps;
        for (const auto& name : currentSettings_.allowedApplications) {
            std::string entry = name;
            size_t pos = entry.find('=');
            if (pos == std::string::npos) {
                std::string key = TrimStringAnsi(entry);
                if (!key.empty()) {
                    apps[key] = "";
                }
            } else {
                std::string key = TrimStringAnsi(entry.substr(0, pos));
                std::string value = TrimStringAnsi(entry.substr(pos + 1));
                if (!key.empty()) {
                    apps[key] = value;
                }
            }
        }
        configManager_->setAllowedApps(apps);
        configManager_->setHighRiskConfirmations(currentSettings_.highRiskConfirmations);
        configManager_->setDashboardAutoShow(currentSettings_.dashboardAutoShow);
        configManager_->setDashboardAlwaysOnTop(currentSettings_.dashboardAlwaysOnTop);
        configManager_->setTrayIconStyle(currentSettings_.trayIconStyle);
        configManager_->setLogRetentionDays(currentSettings_.logRetentionDays);
        configManager_->setDaemonEnabled(currentSettings_.daemonEnabled);
        configManager_->setActualPort(currentSettings_.serverPort);
        configManager_->setAutoPort(currentSettings_.autoPortSelection);
        configManager_->setListenAddress(currentSettings_.listenAddress);
        configManager_->save();

        if (currentSettings_.daemonEnabled != originalSettings_.daemonEnabled) {
            if (currentSettings_.daemonEnabled) {
                StartDaemonIfNeeded();
            } else {
                SignalDaemonExit();
            }
        }
    } catch (const std::exception& e) {
        MessageBoxA(hwnd_, e.what(), "Settings", MB_OK | MB_ICONWARNING);
        return false;
    }

    originalSettings_ = currentSettings_;
    hasUnsavedChanges_ = false;
    return true;
}

bool SettingsWindow::ValidateSettings() {
    if (!currentSettings_.autoPortSelection) {
        if (currentSettings_.serverPort < 1024 || currentSettings_.serverPort > 65535) {
            return false;
        }
    }
    if (currentSettings_.logRetentionDays <= 0) {
        return false;
    }
    return true;
}

void SettingsWindow::RevertChanges() {
    if (localizationManager_ && currentSettings_.language != originalSettings_.language) {
        localizationManager_->setLanguage(originalSettings_.language);
        UpdateTrayIconTooltip();
    }

    if (g_dashboard) {
        if (currentSettings_.dashboardAutoShow != originalSettings_.dashboardAutoShow) {
            if (originalSettings_.dashboardAutoShow) {
                if (!g_dashboard->getHandle()) {
                    g_dashboard->create();
                } else {
                    g_dashboard->show();
                }
            } else if (g_dashboard->getHandle()) {
                g_dashboard->hide();
            }
        }
        if (currentSettings_.dashboardAlwaysOnTop != originalSettings_.dashboardAlwaysOnTop) {
            if (g_dashboard->getHandle()) {
                HWND dash = g_dashboard->getHandle();
                SetWindowPos(dash,
                             originalSettings_.dashboardAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                             0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE);
            }
        }
    }

    currentSettings_ = originalSettings_;
    hasUnsavedChanges_ = false;
}

INT_PTR CALLBACK SettingsWindow::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* window = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == WM_INITDIALOG) {
        window = reinterpret_cast<SettingsWindow*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
        if (window) {
            window->hwnd_ = hwnd;
            window->onInitDialog();
        }
        return TRUE;
    }

    if (!window) {
        return FALSE;
    }

    switch (msg) {
    case WM_NOTIFY: {
        LPNMHDR hdr = reinterpret_cast<LPNMHDR>(lParam);
        if (hdr && hdr->idFrom == IDC_SETTINGS_TAB && hdr->code == TCN_SELCHANGE) {
            int index = TabCtrl_GetCurSel(window->tabControl_);
            HWND languageTab = GetDlgItem(window->tabControl_, IDC_TAB_LANGUAGE);
            HWND startupTab = GetDlgItem(window->tabControl_, IDC_TAB_STARTUP);
            HWND apiKeyTab = GetDlgItem(window->tabControl_, IDC_TAB_APIKEY);
            HWND serverTab = GetDlgItem(window->tabControl_, IDC_TAB_SERVER);
            HWND appearanceTab = GetDlgItem(window->tabControl_, IDC_TAB_APPEARANCE);
            HWND aboutTab = GetDlgItem(window->tabControl_, IDC_TAB_ABOUT);
            if (languageTab) ShowWindow(languageTab, index == 0 ? SW_SHOW : SW_HIDE);
            if (startupTab) ShowWindow(startupTab, index == 1 ? SW_SHOW : SW_HIDE);
            if (apiKeyTab) ShowWindow(apiKeyTab, index == 2 ? SW_SHOW : SW_HIDE);
            if (serverTab) ShowWindow(serverTab, index == 3 ? SW_SHOW : SW_HIDE);
            if (appearanceTab) ShowWindow(appearanceTab, index == 4 ? SW_SHOW : SW_HIDE);
            if (aboutTab) ShowWindow(aboutTab, index == 5 ? SW_SHOW : SW_HIDE);
        }
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_LANGUAGE_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE && window->localizationManager_) {
                HWND combo = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_LANGUAGE), IDC_LANGUAGE_COMBO);
                int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR) {
                    auto languages = window->localizationManager_->getSupportedLanguages();
                    if (sel >= 0 && sel < static_cast<int>(languages.size())) {
                        window->currentSettings_.language = languages[sel].code;
                        window->localizationManager_->setLanguage(languages[sel].code);
                        window->hasUnsavedChanges_ = true;
                        window->updateTabTitles();
                        UpdateTrayIconTooltip();
                        if (g_dashboard) {
                            g_dashboard->refreshLocalization();
                        }
                        // Save language change immediately
                        if (window->configManager_) {
                            window->configManager_->setLanguage(languages[sel].code);
                            window->configManager_->save();
                        }
                    }
                }
            }
            return TRUE;
        case IDC_STARTUP_CHECKBOX: {
            HWND check = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_STARTUP), IDC_STARTUP_CHECKBOX);
            if (check) {
                LRESULT state = SendMessage(check, BM_GETCHECK, 0, 0);
                window->currentSettings_.autoStartup = (state == BST_CHECKED);
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        }
        case IDC_APIKEY_COPY: {
            std::wstring wideApi(window->currentSettings_.apiKey.begin(), window->currentSettings_.apiKey.end());
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                size_t bytes = (wideApi.size() + 1) * sizeof(wchar_t);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                if (hMem) {
                    void* ptr = GlobalLock(hMem);
                    if (ptr) {
                        memcpy(ptr, wideApi.c_str(), bytes);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    } else {
                        GlobalFree(hMem);
                    }
                }
                CloseClipboard();
            }
            return TRUE;
        }
        case IDC_APIKEY_REGEN: {
            int confirm = MessageBox(hwnd, L"Regenerate API key?", L"Settings", MB_OKCANCEL | MB_ICONWARNING);
            if (confirm == IDOK) {
                if (window->licenseManager_) {
                    window->currentSettings_.apiKey = window->licenseManager_->generateApiKey();
                } else {
                    window->currentSettings_.apiKey = window->currentSettings_.apiKey + "x";
                }
                window->hasUnsavedChanges_ = true;
                window->updateTabTitles();
            }
            return TRUE;
        }
        case IDC_APIKEY_INPUT:
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t buffer[512];
                GetWindowTextW(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APIKEY), IDC_APIKEY_INPUT),
                               buffer, 512);
                std::wstring input = buffer;
                std::string apiKey(input.begin(), input.end());
                if (!apiKey.empty()) {
                    window->currentSettings_.apiKey = apiKey;
                    window->hasUnsavedChanges_ = true;
                    bool valid = window->licenseManager_ ? window->licenseManager_->validateApiKeyFormat(apiKey) : false;
                    window->currentSettings_.apiKeyValid = valid;
                    std::wstring statusText = valid
                        ? (window->localizationManager_ ? window->localizationManager_->getString("apikey.status.valid") : L"Status: valid")
                        : (window->localizationManager_ ? window->localizationManager_->getString("apikey.status.invalid") : L"Status: invalid");
                    SetWindowTextW(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APIKEY), IDC_APIKEY_STATUS),
                                   statusText.c_str());
                    window->updateTabTitles();
                }
            }
            return TRUE;
        case IDC_SERVER_AUTOPORT: {
            HWND check = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SERVER), IDC_SERVER_AUTOPORT);
            if (check) {
                LRESULT state = SendMessage(check, BM_GETCHECK, 0, 0);
                window->currentSettings_.autoPortSelection = (state == BST_CHECKED);
                EnableWindow(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SERVER), IDC_SERVER_PORT_INPUT),
                             window->currentSettings_.autoPortSelection ? FALSE : TRUE);
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        }
        case IDC_SERVER_LISTEN_ALL:
        case IDC_SERVER_LISTEN_LOCAL: {
            bool listenAll = (LOWORD(wParam) == IDC_SERVER_LISTEN_ALL);
            window->currentSettings_.listenAddress = listenAll ? "0.0.0.0" : "127.0.0.1";
            window->hasUnsavedChanges_ = true;
            return TRUE;
        }
        case IDC_SERVER_PORT_INPUT:
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t buffer[16];
                GetWindowTextW(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SERVER), IDC_SERVER_PORT_INPUT),
                               buffer, 16);
                int port = _wtoi(buffer);
                window->currentSettings_.serverPort = port;
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_TOKEN_REGEN: {
            int confirm = MessageBox(hwnd, L"Regenerate token?", L"Settings", MB_OKCANCEL | MB_ICONWARNING);
            if (confirm == IDOK) {
                static const char kHex[] = "0123456789abcdef";
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 15);
                std::string token;
                token.reserve(64);
                for (int i = 0; i < 64; ++i) {
                    token.push_back(kHex[dis(gen)]);
                }
                window->currentSettings_.bearerToken = token;
                window->hasUnsavedChanges_ = true;
                window->updateTabTitles();
            }
            return TRUE;
        }
        case IDC_SECURITY_HIGH_RISK_CHECK: {
            HWND check = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY), IDC_SECURITY_HIGH_RISK_CHECK);
            if (check) {
                LRESULT state = SendMessage(check, BM_GETCHECK, 0, 0);
                window->currentSettings_.highRiskConfirmations = (state == BST_CHECKED);
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_DIRS_ADD: {
            BROWSEINFOW bi{};
            wchar_t displayName[MAX_PATH] = {0};
            bi.hwndOwner = hwnd;
            bi.pszDisplayName = displayName;
            bi.lpszTitle = window->localizationManager_
                ? window->localizationManager_->getString("security.add_dir.title").c_str()
                : L"Select directory";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH] = {0};
                if (SHGetPathFromIDListW(pidl, path)) {
                    std::wstring value = path;
                    HWND list = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                           IDC_SECURITY_ALLOWED_DIRS_LIST);
                    if (!IsDirectoryPath(value)) {
                        ShowValidationError(hwnd, window->localizationManager_,
                                            L"Selected path is not a directory.",
                                            "security.error.dir_required");
                    } else if (ListBoxContains(list, value)) {
                        ShowValidationError(hwnd, window->localizationManager_,
                                            L"Entry already exists.",
                                            "security.error.duplicate");
                    } else {
                        AddListItem(list, value);
                        window->syncSecurityListsFromControls();
                        window->updateSecurityButtonsState();
                        window->hasUnsavedChanges_ = true;
                    }
                }
                CoTaskMemFree(pidl);
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_DIRS_EDIT: {
            HWND list = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                   IDC_SECURITY_ALLOWED_DIRS_LIST);
            int index = GetSelectedIndex(list);
            if (index == LB_ERR) {
                ShowValidationError(hwnd, window->localizationManager_,
                                    L"Select an item first.",
                                    "security.error.select_item");
                return TRUE;
            }
            BROWSEINFOW bi{};
            wchar_t displayName[MAX_PATH] = {0};
            bi.hwndOwner = hwnd;
            bi.pszDisplayName = displayName;
            bi.lpszTitle = window->localizationManager_
                ? window->localizationManager_->getString("security.edit_dir.title").c_str()
                : L"Select directory";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH] = {0};
                if (SHGetPathFromIDListW(pidl, path)) {
                    std::wstring value = path;
                    if (!IsDirectoryPath(value)) {
                        ShowValidationError(hwnd, window->localizationManager_,
                                            L"Selected path is not a directory.",
                                            "security.error.dir_required");
                    } else if (ListBoxContains(list, value, index)) {
                        ShowValidationError(hwnd, window->localizationManager_,
                                            L"Entry already exists.",
                                            "security.error.duplicate");
                    } else {
                        SendMessageW(list, LB_DELETESTRING, index, 0);
                        SendMessageW(list, LB_INSERTSTRING, index, (LPARAM)value.c_str());
                        SendMessageW(list, LB_SETCURSEL, index, 0);
                        window->syncSecurityListsFromControls();
                        window->updateSecurityButtonsState();
                        window->hasUnsavedChanges_ = true;
                    }
                }
                CoTaskMemFree(pidl);
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_DIRS_REMOVE:
            if (RemoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                  IDC_SECURITY_ALLOWED_DIRS_LIST))) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            } else {
                ShowValidationError(hwnd, window->localizationManager_,
                                    L"Select an item first.",
                                    "security.error.select_item");
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_DIRS_UP:
            if (MoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                IDC_SECURITY_ALLOWED_DIRS_LIST), -1)) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_DIRS_DOWN:
            if (MoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                IDC_SECURITY_ALLOWED_DIRS_LIST), 1)) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_APPS_ADD: {
            wchar_t filePath[MAX_PATH] = {0};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            std::wstring addAppTitle = window->localizationManager_
                ? window->localizationManager_->getString("security.add_app.title")
                : L"Select application";
            ofn.lpstrTitle = addAppTitle.c_str();
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                std::wstring value = filePath;
                HWND list = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                       IDC_SECURITY_ALLOWED_APPS_LIST);
                if (!IsFilePath(value)) {
                    ShowValidationError(hwnd, window->localizationManager_,
                                        L"Selected path is not a file.",
                                        "security.error.file_required");
                } else {
                    std::wstring entry = FormatAppEntry(value);
                    if (AppEntryConflicts(list, entry)) {
                        ShowValidationError(hwnd, window->localizationManager_,
                                            L"Entry already exists.",
                                            "security.error.duplicate");
                    } else {
                        AddListItem(list, entry);
                        window->syncSecurityListsFromControls();
                        window->updateSecurityButtonsState();
                        window->hasUnsavedChanges_ = true;
                    }
                }
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_APPS_EDIT: {
            HWND list = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                   IDC_SECURITY_ALLOWED_APPS_LIST);
            int index = GetSelectedIndex(list);
            if (index == LB_ERR) {
                ShowValidationError(hwnd, window->localizationManager_,
                                    L"Select an item first.",
                                    "security.error.select_item");
                return TRUE;
            }
            std::wstring currentEntry = GetSelectedText(list, index);
            std::wstring currentName;
            std::wstring currentPath;
            SplitAppEntryW(currentEntry, &currentName, &currentPath);
            wchar_t filePath[MAX_PATH] = {0};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            std::wstring editAppTitle = window->localizationManager_
                ? window->localizationManager_->getString("security.edit_app.title")
                : L"Select application";
            ofn.lpstrTitle = editAppTitle.c_str();
            wchar_t initialDir[MAX_PATH] = {0};
            if (!currentPath.empty() && currentPath.size() < MAX_PATH) {
                wcscpy_s(initialDir, currentPath.c_str());
                PathRemoveFileSpecW(initialDir);
                ofn.lpstrInitialDir = initialDir;
            }
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                std::wstring value = filePath;
                if (!IsFilePath(value)) {
                    ShowValidationError(hwnd, window->localizationManager_,
                                        L"Selected path is not a file.",
                                        "security.error.file_required");
                } else {
                    std::wstring entry = FormatAppEntry(value);
                    if (AppEntryConflicts(list, entry, index)) {
                        ShowValidationError(hwnd, window->localizationManager_,
                                            L"Entry already exists.",
                                            "security.error.duplicate");
                    } else {
                        SendMessageW(list, LB_DELETESTRING, index, 0);
                        SendMessageW(list, LB_INSERTSTRING, index, (LPARAM)entry.c_str());
                        SendMessageW(list, LB_SETCURSEL, index, 0);
                        window->syncSecurityListsFromControls();
                        window->updateSecurityButtonsState();
                        window->hasUnsavedChanges_ = true;
                    }
                }
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_APPS_REMOVE:
            if (RemoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                  IDC_SECURITY_ALLOWED_APPS_LIST))) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            } else {
                ShowValidationError(hwnd, window->localizationManager_,
                                    L"Select an item first.",
                                    "security.error.select_item");
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_APPS_UP:
            if (MoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                IDC_SECURITY_ALLOWED_APPS_LIST), -1)) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_APPS_DOWN:
            if (MoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                IDC_SECURITY_ALLOWED_APPS_LIST), 1)) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_COMMANDS_ADD: {
            std::wstring title = window->localizationManager_
                ? window->localizationManager_->getString("security.add_command.title")
                : L"Add command";
            std::wstring label = window->localizationManager_
                ? window->localizationManager_->getString("security.add_command.label")
                : L"Command:";
            std::wstring value;
            if (PromptForText(hwnd, title, label, &value)) {
                value = TrimString(value);
                HWND list = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                       IDC_SECURITY_ALLOWED_COMMANDS_LIST);
                if (value.empty()) {
                    ShowValidationError(hwnd, window->localizationManager_,
                                        L"Command cannot be empty.",
                                        "security.error.empty");
                } else if (ListBoxContains(list, value)) {
                    ShowValidationError(hwnd, window->localizationManager_,
                                        L"Entry already exists.",
                                        "security.error.duplicate");
                } else {
                    AddListItem(list, value);
                    window->syncSecurityListsFromControls();
                    window->updateSecurityButtonsState();
                    window->hasUnsavedChanges_ = true;
                }
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_COMMANDS_EDIT: {
            HWND list = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                   IDC_SECURITY_ALLOWED_COMMANDS_LIST);
            int index = GetSelectedIndex(list);
            if (index == LB_ERR) {
                ShowValidationError(hwnd, window->localizationManager_,
                                    L"Select an item first.",
                                    "security.error.select_item");
                return TRUE;
            }
            std::wstring current = GetSelectedText(list, index);
            std::wstring title = window->localizationManager_
                ? window->localizationManager_->getString("security.edit_command.title")
                : L"Edit command";
            std::wstring label = window->localizationManager_
                ? window->localizationManager_->getString("security.add_command.label")
                : L"Command:";
            std::wstring value = current;
            if (PromptForText(hwnd, title, label, &value)) {
                value = TrimString(value);
                if (value.empty()) {
                    ShowValidationError(hwnd, window->localizationManager_,
                                        L"Command cannot be empty.",
                                        "security.error.empty");
                } else if (ListBoxContains(list, value, index)) {
                    ShowValidationError(hwnd, window->localizationManager_,
                                        L"Entry already exists.",
                                        "security.error.duplicate");
                } else {
                    SendMessageW(list, LB_DELETESTRING, index, 0);
                    SendMessageW(list, LB_INSERTSTRING, index, (LPARAM)value.c_str());
                    SendMessageW(list, LB_SETCURSEL, index, 0);
                    window->syncSecurityListsFromControls();
                    window->updateSecurityButtonsState();
                    window->hasUnsavedChanges_ = true;
                }
            }
            return TRUE;
        }
        case IDC_SECURITY_ALLOWED_COMMANDS_REMOVE:
            if (RemoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                  IDC_SECURITY_ALLOWED_COMMANDS_LIST))) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            } else {
                ShowValidationError(hwnd, window->localizationManager_,
                                    L"Select an item first.",
                                    "security.error.select_item");
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_COMMANDS_UP:
            if (MoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                IDC_SECURITY_ALLOWED_COMMANDS_LIST), -1)) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_COMMANDS_DOWN:
            if (MoveSelectedListItem(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_SECURITY),
                                                IDC_SECURITY_ALLOWED_COMMANDS_LIST), 1)) {
                window->syncSecurityListsFromControls();
                window->updateSecurityButtonsState();
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_SECURITY_ALLOWED_DIRS_LIST:
        case IDC_SECURITY_ALLOWED_APPS_LIST:
        case IDC_SECURITY_ALLOWED_COMMANDS_LIST:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                window->updateSecurityButtonsState();
            }
            if (HIWORD(wParam) == LBN_DBLCLK) {
                switch (LOWORD(wParam)) {
                case IDC_SECURITY_ALLOWED_DIRS_LIST:
                    SendMessage(hwnd, WM_COMMAND, IDC_SECURITY_ALLOWED_DIRS_EDIT, 0);
                    break;
                case IDC_SECURITY_ALLOWED_APPS_LIST:
                    SendMessage(hwnd, WM_COMMAND, IDC_SECURITY_ALLOWED_APPS_EDIT, 0);
                    break;
                case IDC_SECURITY_ALLOWED_COMMANDS_LIST:
                    SendMessage(hwnd, WM_COMMAND, IDC_SECURITY_ALLOWED_COMMANDS_EDIT, 0);
                    break;
                default:
                    break;
                }
            }
            return TRUE;
        case IDC_APPEARANCE_DASH_AUTO_SHOW: {
            HWND check = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APPEARANCE), IDC_APPEARANCE_DASH_AUTO_SHOW);
            if (check) {
                LRESULT state = SendMessage(check, BM_GETCHECK, 0, 0);
                window->currentSettings_.dashboardAutoShow = (state == BST_CHECKED);
                if (g_dashboard) {
                    if (window->currentSettings_.dashboardAutoShow) {
                        if (!g_dashboard->getHandle()) {
                            g_dashboard->create();
                        } else {
                            g_dashboard->show();
                        }
                    } else if (g_dashboard->getHandle()) {
                        g_dashboard->hide();
                    }
                }
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        }
        case IDC_APPEARANCE_ALWAYS_ON_TOP: {
            HWND check = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APPEARANCE), IDC_APPEARANCE_ALWAYS_ON_TOP);
            if (check) {
                LRESULT state = SendMessage(check, BM_GETCHECK, 0, 0);
                window->currentSettings_.dashboardAlwaysOnTop = (state == BST_CHECKED);
                if (g_dashboard && g_dashboard->getHandle()) {
                    HWND dash = g_dashboard->getHandle();
                    SetWindowPos(dash,
                                 window->currentSettings_.dashboardAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                                 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE);
                }
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        }
        case IDC_APPEARANCE_TRAY_STYLE_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                HWND combo = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APPEARANCE), IDC_APPEARANCE_TRAY_STYLE_COMBO);
                int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                window->currentSettings_.trayIconStyle = sel == 1 ? "minimal" : "normal";
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_APPEARANCE_LOG_RETENTION_INPUT:
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t buffer[16];
                GetWindowTextW(GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APPEARANCE),
                                          IDC_APPEARANCE_LOG_RETENTION_INPUT),
                               buffer, 16);
                int days = _wtoi(buffer);
                window->currentSettings_.logRetentionDays = days;
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        case IDC_APPEARANCE_DAEMON_ENABLED: {
            HWND check = GetDlgItem(GetDlgItem(window->tabControl_, IDC_TAB_APPEARANCE),
                                    IDC_APPEARANCE_DAEMON_ENABLED);
            if (check) {
                LRESULT state = SendMessage(check, BM_GETCHECK, 0, 0);
                window->currentSettings_.daemonEnabled = (state == BST_CHECKED);
                window->hasUnsavedChanges_ = true;
            }
            return TRUE;
        }
        case IDC_ABOUT_DOCS_LINK:
            ShellExecuteW(hwnd, L"open", L"https://github.com/codyard/WinBridgeAgent", NULL, NULL, SW_SHOWNORMAL);
            return TRUE;
        case IDC_ABOUT_SUPPORT_LINK:
            ShellExecuteW(hwnd, L"open", L"https://github.com/codyard/WinBridgeAgent/issues", NULL, NULL, SW_SHOWNORMAL);
            return TRUE;
        case IDOK:
            window->onOkClicked();
            return TRUE;
        case IDCANCEL:
            window->onCancelClicked();
            return TRUE;
        case IDC_BUTTON_APPLY:
            window->onApplyClicked();
            return TRUE;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    return FALSE;
}

void SettingsWindow::onInitDialog() {
    InitCommonControls();
    tabControl_ = GetDlgItem(hwnd_, IDC_SETTINGS_TAB);
    LoadSettings();
    initializeTabs();
    updateTabTitles();
}

void SettingsWindow::onApplyClicked() {
    syncControlsToSettings();
    if (!ValidateSettings()) {
        MessageBox(hwnd_, L"Invalid settings.", L"Settings", MB_OK | MB_ICONWARNING);
        return;
    }
    if (currentSettings_.autoStartup != originalSettings_.autoStartup) {
        AutoStartupManager manager;
        wchar_t path[MAX_PATH] = {0};
        GetModuleFileNameW(NULL, path, MAX_PATH);
        bool ok = currentSettings_.autoStartup
            ? manager.enableAutoStartup(path)
            : manager.disableAutoStartup();
        if (!ok) {
            MessageBox(hwnd_, L"Unable to modify startup settings.", L"Settings", MB_OK | MB_ICONWARNING);
            currentSettings_.autoStartup = originalSettings_.autoStartup;
        }
    }
    SaveSettings();
    if (g_auditLogger && currentSettings_.logRetentionDays > 0) {
        g_auditLogger->cleanupOldLogs(currentSettings_.logRetentionDays);
    }
}

void SettingsWindow::onOkClicked() {
    syncControlsToSettings();
    if (!ValidateSettings()) {
        MessageBox(hwnd_, L"Invalid settings.", L"Settings", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!SaveSettings()) {
        return;
    }
    dialogResult_ = true;
    DestroyWindow(hwnd_);
}

void SettingsWindow::onCancelClicked() {
    RevertChanges();
    dialogResult_ = false;
    DestroyWindow(hwnd_);
}

void SettingsWindow::initializeTabs() {
    if (!tabControl_) {
        return;
    }
    TCITEMW item{};
    item.mask = TCIF_TEXT;

    const wchar_t* tabs[] = {
        L"Language", L"Startup", L"API Key", L"Server", L"Appearance", L"About"
    };
    for (int i = 0; i < 6; ++i) {
        item.pszText = const_cast<LPWSTR>(tabs[i]);
        SendMessageW(tabControl_, TCM_INSERTITEMW, i, (LPARAM)&item);
    }

    RECT tabRect;
    GetClientRect(tabControl_, &tabRect);
    TabCtrl_AdjustRect(tabControl_, FALSE, &tabRect);

    HWND languageTab = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD | WS_VISIBLE,
        tabRect.left,
        tabRect.top,
        tabRect.right - tabRect.left,
        tabRect.bottom - tabRect.top,
        tabControl_,
        (HMENU)IDC_TAB_LANGUAGE,
        hInstance_,
        NULL);
    AttachTabPageHandler(languageTab, this);

    HWND startupTab = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD,
        tabRect.left,
        tabRect.top,
        tabRect.right - tabRect.left,
        tabRect.bottom - tabRect.top,
        tabControl_,
        (HMENU)IDC_TAB_STARTUP,
        hInstance_,
        NULL);
    AttachTabPageHandler(startupTab, this);

    HWND apiKeyTab = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD,
        tabRect.left,
        tabRect.top,
        tabRect.right - tabRect.left,
        tabRect.bottom - tabRect.top,
        tabControl_,
        (HMENU)IDC_TAB_APIKEY,
        hInstance_,
        NULL);
    AttachTabPageHandler(apiKeyTab, this);

    HWND serverTab = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD,
        tabRect.left,
        tabRect.top,
        tabRect.right - tabRect.left,
        tabRect.bottom - tabRect.top,
        tabControl_,
        (HMENU)IDC_TAB_SERVER,
        hInstance_,
        NULL);
    AttachTabPageHandler(serverTab, this);

    // Security tab hidden in open-source edition

    HWND appearanceTab = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD,
        tabRect.left,
        tabRect.top,
        tabRect.right - tabRect.left,
        tabRect.bottom - tabRect.top,
        tabControl_,
        (HMENU)IDC_TAB_APPEARANCE,
        hInstance_,
        NULL);
    AttachTabPageHandler(appearanceTab, this);

    HWND aboutTab = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD,
        tabRect.left,
        tabRect.top,
        tabRect.right - tabRect.left,
        tabRect.bottom - tabRect.top,
        tabControl_,
        (HMENU)IDC_TAB_ABOUT,
        hInstance_,
        NULL);
    AttachTabPageHandler(aboutTab, this);

    // Language tab controls
    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        12,
        140,
        18,
        languageTab,
        (HMENU)IDC_LANGUAGE_LABEL,
        hInstance_,
        NULL);

    HWND combo = CreateWindowEx(
        0,
        L"COMBOBOX",
        NULL,
        CBS_DROPDOWNLIST | CBS_DISABLENOSCROLL | WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL,
        180,
        10,
        260,
        200,
        languageTab,
        (HMENU)IDC_LANGUAGE_COMBO,
        hInstance_,
        NULL);

    if (combo && localizationManager_) {
        auto languages = localizationManager_->getSupportedLanguages();
        int selectedIndex = 0;
        std::string currentLang = currentSettings_.language.empty() ? localizationManager_->getCurrentLanguage()
                                                                    : currentSettings_.language;
        for (size_t i = 0; i < languages.size(); ++i) {
            const auto& info = languages[i];
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)info.nativeName.c_str());
            if (info.code == currentLang) {
                selectedIndex = static_cast<int>(i);
            }
        }
        SendMessageW(combo, CB_SETCURSEL, selectedIndex, 0);
    }

    // Startup tab controls
    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16,
        12,
        340,
        18,
        startupTab,
        (HMENU)IDC_STARTUP_CHECKBOX,
        hInstance_,
        NULL);

    // API Key tab controls
    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        12,
        140,
        18,
        apiKeyTab,
        (HMENU)IDC_APIKEY_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_READONLY,
        180,
        10,
        220,
        20,
        apiKeyTab,
        (HMENU)IDC_APIKEY_MASKED,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        410,
        10,
        70,
        20,
        apiKeyTab,
        (HMENU)IDC_APIKEY_COPY,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        410,
        34,
        70,
        20,
        apiKeyTab,
        (HMENU)IDC_APIKEY_REGEN,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        42,
        140,
        18,
        apiKeyTab,
        (HMENU)IDC_APIKEY_INPUT_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        180,
        40,
        220,
        20,
        apiKeyTab,
        (HMENU)IDC_APIKEY_INPUT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        70,
        420,
        18,
        apiKeyTab,
        (HMENU)IDC_APIKEY_STATUS,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"https://example.com",
        WS_CHILD | WS_VISIBLE,
        16,
        92,
        420,
        18,
        apiKeyTab,
        (HMENU)IDC_APIKEY_LINK,
        hInstance_,
        NULL);

    // Server tab controls
    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        12,
        140,
        18,
        serverTab,
        (HMENU)IDC_SERVER_PORT_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        180,
        10,
        100,
        20,
        serverTab,
        (HMENU)IDC_SERVER_PORT_INPUT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16,
        38,
        280,
        18,
        serverTab,
        (HMENU)IDC_SERVER_AUTOPORT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        66,
        140,
        18,
        serverTab,
        (HMENU)IDC_SERVER_LISTEN_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"0.0.0.0",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        180,
        64,
        120,
        18,
        serverTab,
        (HMENU)IDC_SERVER_LISTEN_ALL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"127.0.0.1",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        310,
        64,
        120,
        18,
        serverTab,
        (HMENU)IDC_SERVER_LISTEN_LOCAL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        96,
        420,
        18,
        serverTab,
        (HMENU)IDC_SERVER_STATUS_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        118,
        420,
        18,
        serverTab,
        (HMENU)IDC_SERVER_UPTIME_LABEL,
        hInstance_,
        NULL);

    // Security tab hidden in open-source edition
    HWND securityTab = nullptr;
    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        12,
        140,
        18,
        securityTab,
        (HMENU)IDC_SECURITY_TOKEN_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_READONLY,
        180,
        10,
        220,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_TOKEN_MASKED,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        410,
        10,
        70,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_TOKEN_REGEN,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        12,
        12,
        220,
        18,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        12,
        32,
        230,
        120,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_LIST,
        hInstance_,
        NULL);
    SetListBoxItemHeight(GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_LIST), 18);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        12,
        158,
        70,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_UP,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        88,
        158,
        70,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_DOWN,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        164,
        158,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_ADD,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        12,
        182,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_EDIT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        96,
        182,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_DIRS_REMOVE,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        254,
        12,
        220,
        18,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        254,
        32,
        230,
        120,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_LIST,
        hInstance_,
        NULL);
    SetListBoxItemHeight(GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_LIST), 18);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        254,
        158,
        70,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_UP,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        330,
        158,
        70,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_DOWN,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        406,
        158,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_ADD,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        254,
        182,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_EDIT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        338,
        182,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_APPS_REMOVE,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        12,
        212,
        220,
        18,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        12,
        232,
        472,
        120,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_LIST,
        hInstance_,
        NULL);
    SetListBoxItemHeight(GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_LIST), 18);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        12,
        360,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_UP,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        96,
        360,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_DOWN,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        180,
        360,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_ADD,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        264,
        360,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_EDIT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        348,
        360,
        78,
        20,
        securityTab,
        (HMENU)IDC_SECURITY_ALLOWED_COMMANDS_REMOVE,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        12,
        390,
        470,
        18,
        securityTab,
        (HMENU)IDC_SECURITY_HIGH_RISK_CHECK,
        hInstance_,
        NULL);

    // Appearance tab controls
    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16,
        12,
        320,
        18,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_DASH_AUTO_SHOW,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16,
        36,
        320,
        18,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_ALWAYS_ON_TOP,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        64,
        160,
        18,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_TRAY_STYLE_LABEL,
        hInstance_,
        NULL);

    HWND trayCombo = CreateWindowEx(
        0,
        L"COMBOBOX",
        NULL,
        CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
        180,
        62,
        160,
        120,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_TRAY_STYLE_COMBO,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        92,
        160,
        18,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_LOG_RETENTION_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        180,
        90,
        80,
        20,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_LOG_RETENTION_INPUT,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16,
        120,
        360,
        18,
        appearanceTab,
        (HMENU)IDC_APPEARANCE_DAEMON_ENABLED,
        hInstance_,
        NULL);

    if (trayCombo && localizationManager_) {
        SendMessageW(trayCombo, CB_ADDSTRING, 0, (LPARAM)localizationManager_->getString("appearance.tray.normal").c_str());
        SendMessageW(trayCombo, CB_ADDSTRING, 0, (LPARAM)localizationManager_->getString("appearance.tray.minimal").c_str());
    }

    // About tab controls
    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        12,
        420,
        18,
        aboutTab,
        (HMENU)IDC_ABOUT_VERSION_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        34,
        420,
        18,
        aboutTab,
        (HMENU)IDC_ABOUT_LICENSE_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        56,
        420,
        18,
        aboutTab,
        (HMENU)IDC_ABOUT_EXPIRES_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        78,
        420,
        18,
        aboutTab,
        (HMENU)IDC_ABOUT_OS_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        100,
        420,
        18,
        aboutTab,
        (HMENU)IDC_ABOUT_ARCH_LABEL,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        16,
        130,
        140,
        20,
        aboutTab,
        (HMENU)IDC_ABOUT_DOCS_LINK,
        hInstance_,
        NULL);

    CreateWindowEx(
        0,
        L"BUTTON",
        L"",
        WS_CHILD | WS_VISIBLE,
        166,
        130,
        140,
        20,
        aboutTab,
        (HMENU)IDC_ABOUT_SUPPORT_LINK,
        hInstance_,
        NULL);

    ShowWindow(languageTab, SW_SHOW);
    ShowWindow(startupTab, SW_HIDE);
    ShowWindow(apiKeyTab, SW_HIDE);
    ShowWindow(serverTab, SW_HIDE);
    ShowWindow(securityTab, SW_HIDE);
    ShowWindow(appearanceTab, SW_HIDE);
    ShowWindow(aboutTab, SW_HIDE);
}

void SettingsWindow::updateTabTitles() {
    if (!tabControl_) {
        return;
    }
    if (hwnd_) {
        std::wstring title = localizationManager_ ? localizationManager_->getString("settings.title") : L"Settings";
        SetWindowTextW(hwnd_, title.c_str());

        HWND okButton = GetDlgItem(hwnd_, IDOK);
        if (okButton) {
            SetWindowTextW(okButton,
                           localizationManager_ ? localizationManager_->getString("settings.button.ok").c_str() : L"OK");
        }
        HWND cancelButton = GetDlgItem(hwnd_, IDCANCEL);
        if (cancelButton) {
            SetWindowTextW(cancelButton,
                           localizationManager_ ? localizationManager_->getString("settings.button.cancel").c_str() : L"Cancel");
        }
        HWND applyButton = GetDlgItem(hwnd_, IDC_BUTTON_APPLY);
        if (applyButton) {
            SetWindowTextW(applyButton,
                           localizationManager_ ? localizationManager_->getString("settings.button.apply").c_str() : L"Apply");
        }
    }
    TCITEMW item{};
    item.mask = TCIF_TEXT;

    std::vector<std::wstring> labels = {
        localizationManager_ ? localizationManager_->getString("settings.tab.language") : L"Language",
        localizationManager_ ? localizationManager_->getString("settings.tab.startup") : L"Startup",
        localizationManager_ ? localizationManager_->getString("settings.tab.apikey") : L"API Key",
        localizationManager_ ? localizationManager_->getString("settings.tab.server") : L"Server",
        localizationManager_ ? localizationManager_->getString("settings.tab.appearance") : L"Appearance",
        localizationManager_ ? localizationManager_->getString("settings.tab.about") : L"About"
    };

    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        item.pszText = const_cast<LPWSTR>(labels[i].c_str());
        SendMessageW(tabControl_, TCM_SETITEMW, i, (LPARAM)&item);
    }

    HWND languageTab = GetDlgItem(tabControl_, IDC_TAB_LANGUAGE);
    HWND startupTab = GetDlgItem(tabControl_, IDC_TAB_STARTUP);
    HWND apiKeyTab = GetDlgItem(tabControl_, IDC_TAB_APIKEY);
    HWND serverTab = GetDlgItem(tabControl_, IDC_TAB_SERVER);
    HWND appearanceTab = GetDlgItem(tabControl_, IDC_TAB_APPEARANCE);
    HWND aboutTab = GetDlgItem(tabControl_, IDC_TAB_ABOUT);
    if (languageTab) {
        SetWindowTextW(GetDlgItem(languageTab, IDC_LANGUAGE_LABEL),
                       localizationManager_ ? localizationManager_->getString("language.select").c_str() : L"Select Language:");
    }
    if (startupTab) {
        SetWindowTextW(GetDlgItem(startupTab, IDC_STARTUP_CHECKBOX),
                       localizationManager_ ? localizationManager_->getString("startup.auto").c_str() : L"Start with Windows");
        SendMessageW(GetDlgItem(startupTab, IDC_STARTUP_CHECKBOX), BM_SETCHECK,
                     currentSettings_.autoStartup ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (apiKeyTab) {
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_LABEL),
                       localizationManager_ ? localizationManager_->getString("apikey.current").c_str() : L"Current API Key:");
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_INPUT_LABEL),
                       localizationManager_ ? localizationManager_->getString("apikey.new").c_str() : L"New API Key:");
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_COPY),
                       localizationManager_ ? localizationManager_->getString("apikey.copy").c_str() : L"Copy");
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_REGEN),
                       localizationManager_ ? localizationManager_->getString("apikey.regenerate").c_str() : L"Regenerate");
        std::string maskedApi = MaskSensitiveData(currentSettings_.apiKey);
        std::wstring masked(maskedApi.begin(), maskedApi.end());
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_MASKED), masked.c_str());
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_INPUT), L"");
        SetWindowTextW(GetDlgItem(apiKeyTab, IDC_APIKEY_STATUS), L"");
    }
    if (serverTab) {
        SetWindowTextW(GetDlgItem(serverTab, IDC_SERVER_PORT_LABEL),
                       localizationManager_ ? localizationManager_->getString("server.port").c_str() : L"Server Port:");
        SetWindowTextW(GetDlgItem(serverTab, IDC_SERVER_AUTOPORT),
                       localizationManager_ ? localizationManager_->getString("server.autoport").c_str() : L"Auto-select port");
        SetWindowTextW(GetDlgItem(serverTab, IDC_SERVER_LISTEN_LABEL),
                       localizationManager_ ? localizationManager_->getString("server.listen").c_str() : L"Listen Address:");
        wchar_t portBuf[16];
        _itow_s(currentSettings_.serverPort, portBuf, 10);
        SetWindowTextW(GetDlgItem(serverTab, IDC_SERVER_PORT_INPUT), portBuf);
        SendMessageW(GetDlgItem(serverTab, IDC_SERVER_AUTOPORT), BM_SETCHECK,
                     currentSettings_.autoPortSelection ? BST_CHECKED : BST_UNCHECKED, 0);
        EnableWindow(GetDlgItem(serverTab, IDC_SERVER_PORT_INPUT), currentSettings_.autoPortSelection ? FALSE : TRUE);
        bool listenAll = currentSettings_.listenAddress == "0.0.0.0";
        SendMessageW(GetDlgItem(serverTab, IDC_SERVER_LISTEN_ALL), BM_SETCHECK, listenAll ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(serverTab, IDC_SERVER_LISTEN_LOCAL), BM_SETCHECK, listenAll ? BST_UNCHECKED : BST_CHECKED, 0);
        SetWindowTextW(GetDlgItem(serverTab, IDC_SERVER_STATUS_LABEL),
                       localizationManager_ ? localizationManager_->getString("server.status").c_str() : L"Status: Running");
        SetWindowTextW(GetDlgItem(serverTab, IDC_SERVER_UPTIME_LABEL),
                       localizationManager_ ? localizationManager_->getString("server.uptime").c_str() : L"Uptime: N/A");
    }
    // Security tab hidden in open-source edition
    if (appearanceTab) {
        SetWindowTextW(GetDlgItem(appearanceTab, IDC_APPEARANCE_DASH_AUTO_SHOW),
                       localizationManager_ ? localizationManager_->getString("appearance.dashboard.autoshow").c_str() : L"Show dashboard on startup");
        SetWindowTextW(GetDlgItem(appearanceTab, IDC_APPEARANCE_ALWAYS_ON_TOP),
                       localizationManager_ ? localizationManager_->getString("appearance.dashboard.always_on_top").c_str() : L"Dashboard always on top");
        SetWindowTextW(GetDlgItem(appearanceTab, IDC_APPEARANCE_TRAY_STYLE_LABEL),
                       localizationManager_ ? localizationManager_->getString("appearance.tray_icon_style").c_str() : L"Tray icon style");
        SetWindowTextW(GetDlgItem(appearanceTab, IDC_APPEARANCE_LOG_RETENTION_LABEL),
                       localizationManager_ ? localizationManager_->getString("appearance.log_retention_days").c_str() : L"Log retention days");
        SetWindowTextW(GetDlgItem(appearanceTab, IDC_APPEARANCE_DAEMON_ENABLED),
                       localizationManager_ ? localizationManager_->getString("appearance.daemon.enabled").c_str() : L"Enable daemon watchdog");
        SendMessageW(GetDlgItem(appearanceTab, IDC_APPEARANCE_DASH_AUTO_SHOW), BM_SETCHECK,
                     currentSettings_.dashboardAutoShow ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(appearanceTab, IDC_APPEARANCE_ALWAYS_ON_TOP), BM_SETCHECK,
                     currentSettings_.dashboardAlwaysOnTop ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(appearanceTab, IDC_APPEARANCE_DAEMON_ENABLED), BM_SETCHECK,
                     currentSettings_.daemonEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
        HWND combo = GetDlgItem(appearanceTab, IDC_APPEARANCE_TRAY_STYLE_COMBO);
        if (combo) {
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            if (localizationManager_) {
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)localizationManager_->getString("appearance.tray.normal").c_str());
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)localizationManager_->getString("appearance.tray.minimal").c_str());
            } else {
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Normal");
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Minimal");
            }
            int index = currentSettings_.trayIconStyle == "minimal" ? 1 : 0;
            SendMessageW(combo, CB_SETCURSEL, index, 0);
        }
        wchar_t daysBuf[16];
        _itow_s(currentSettings_.logRetentionDays, daysBuf, 10);
        SetWindowTextW(GetDlgItem(appearanceTab, IDC_APPEARANCE_LOG_RETENTION_INPUT), daysBuf);
    }
    if (aboutTab) {
        std::wstring version = L"" CLAWDESK_VERSION "";
        std::wstring license = L"Free";
        if (licenseManager_) {
            auto info = licenseManager_->validateLicense();
            if (info.status == LicenseStatus::Active) {
                license = L"Pro";
            } else if (info.status == LicenseStatus::Expired) {
                license = L"Expired";
            } else if (info.status == LicenseStatus::Invalid) {
                license = L"Invalid";
            }
        }

        OSVERSIONINFOEXW osInfo{};
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);
        GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osInfo));
        std::wostringstream osStream;
        osStream << L"Windows " << osInfo.dwMajorVersion << L"." << osInfo.dwMinorVersion;

        SYSTEM_INFO sysInfo{};
        GetNativeSystemInfo(&sysInfo);
        std::wstring arch = L"Unknown";
        if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
            arch = L"x64";
        } else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
            arch = L"x86";
        } else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
            arch = L"ARM64";
        }

        std::wstring expiresText = L"N/A";
        if (licenseManager_) {
            auto exp = licenseManager_->getExpirationTime();
            if (exp.has_value()) {
                std::time_t t = std::chrono::system_clock::to_time_t(exp.value());
                std::tm tm{};
                gmtime_s(&tm, &t);
                wchar_t buffer[32];
                wcsftime(buffer, 32, L"%Y-%m-%d", &tm);
                expiresText = buffer;
            }
        }

        std::wstring versionLabel = (localizationManager_ ? localizationManager_->getString("about.version") : L"Version:") + L" " + version;
        std::wstring licenseLabel = (localizationManager_ ? localizationManager_->getString("about.license") : L"License:") + L" " + license;
        std::wstring expiresLabel = (localizationManager_ ? localizationManager_->getString("about.expires") : L"Expires:") + L" " + expiresText;
        std::wstring osLabel = (localizationManager_ ? localizationManager_->getString("about.os") : L"OS:") + L" " + osStream.str();
        std::wstring archLabel = (localizationManager_ ? localizationManager_->getString("about.arch") : L"Arch:") + L" " + arch;

        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_VERSION_LABEL), versionLabel.c_str());
        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_LICENSE_LABEL), licenseLabel.c_str());
        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_EXPIRES_LABEL), expiresLabel.c_str());
        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_OS_LABEL), osLabel.c_str());
        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_ARCH_LABEL), archLabel.c_str());

        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_DOCS_LINK),
                       localizationManager_ ? localizationManager_->getString("about.docs").c_str() : L"Documentation");
        SetWindowTextW(GetDlgItem(aboutTab, IDC_ABOUT_SUPPORT_LINK),
                       localizationManager_ ? localizationManager_->getString("about.support").c_str() : L"Support");
    }
}

void SettingsWindow::syncControlsToSettings() {
    if (!tabControl_) {
        return;
    }

    HWND languageTab = GetDlgItem(tabControl_, IDC_TAB_LANGUAGE);
    HWND startupTab = GetDlgItem(tabControl_, IDC_TAB_STARTUP);
    HWND apiKeyTab = GetDlgItem(tabControl_, IDC_TAB_APIKEY);
    HWND serverTab = GetDlgItem(tabControl_, IDC_TAB_SERVER);
    HWND securityTab = GetDlgItem(tabControl_, IDC_TAB_SECURITY);
    HWND appearanceTab = GetDlgItem(tabControl_, IDC_TAB_APPEARANCE);

    if (languageTab && localizationManager_) {
        HWND combo = GetDlgItem(languageTab, IDC_LANGUAGE_COMBO);
        int sel = combo ? (int)SendMessageW(combo, CB_GETCURSEL, 0, 0) : CB_ERR;
        if (sel != CB_ERR) {
            auto languages = localizationManager_->getSupportedLanguages();
            if (sel >= 0 && sel < static_cast<int>(languages.size())) {
                currentSettings_.language = languages[sel].code;
            }
        }
    }

    if (startupTab) {
        HWND check = GetDlgItem(startupTab, IDC_STARTUP_CHECKBOX);
        if (check) {
            currentSettings_.autoStartup =
                (SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
    }

    if (apiKeyTab) {
        HWND input = GetDlgItem(apiKeyTab, IDC_APIKEY_INPUT);
        if (input) {
            wchar_t buffer[512];
            GetWindowTextW(input, buffer, 512);
            std::wstring value = TrimString(buffer);
            if (!value.empty()) {
                std::string apiKey(value.begin(), value.end());
                currentSettings_.apiKey = apiKey;
                currentSettings_.apiKeyValid =
                    licenseManager_ ? licenseManager_->validateApiKeyFormat(apiKey) : false;
            }
        }
    }

    if (serverTab) {
        HWND portInput = GetDlgItem(serverTab, IDC_SERVER_PORT_INPUT);
        if (portInput) {
            wchar_t buffer[16];
            GetWindowTextW(portInput, buffer, 16);
            currentSettings_.serverPort = _wtoi(buffer);
        }
        HWND autoPort = GetDlgItem(serverTab, IDC_SERVER_AUTOPORT);
        if (autoPort) {
            currentSettings_.autoPortSelection =
                (SendMessageW(autoPort, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        HWND listenAll = GetDlgItem(serverTab, IDC_SERVER_LISTEN_ALL);
        if (listenAll &&
            SendMessageW(listenAll, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            currentSettings_.listenAddress = "0.0.0.0";
        } else {
            currentSettings_.listenAddress = "127.0.0.1";
        }
    }

    if (securityTab) {
        HWND check = GetDlgItem(securityTab, IDC_SECURITY_HIGH_RISK_CHECK);
        if (check) {
            currentSettings_.highRiskConfirmations =
                (SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        syncSecurityListsFromControls();
    }

    if (appearanceTab) {
        HWND autoShow = GetDlgItem(appearanceTab, IDC_APPEARANCE_DASH_AUTO_SHOW);
        if (autoShow) {
            currentSettings_.dashboardAutoShow =
                (SendMessageW(autoShow, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        HWND alwaysOnTop = GetDlgItem(appearanceTab, IDC_APPEARANCE_ALWAYS_ON_TOP);
        if (alwaysOnTop) {
            currentSettings_.dashboardAlwaysOnTop =
                (SendMessageW(alwaysOnTop, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        HWND combo = GetDlgItem(appearanceTab, IDC_APPEARANCE_TRAY_STYLE_COMBO);
        if (combo) {
            int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
            currentSettings_.trayIconStyle = sel == 1 ? "minimal" : "normal";
        }
        HWND logInput = GetDlgItem(appearanceTab, IDC_APPEARANCE_LOG_RETENTION_INPUT);
        if (logInput) {
            wchar_t buffer[16];
            GetWindowTextW(logInput, buffer, 16);
            currentSettings_.logRetentionDays = _wtoi(buffer);
        }
        HWND daemonCheck = GetDlgItem(appearanceTab, IDC_APPEARANCE_DAEMON_ENABLED);
        if (daemonCheck) {
            currentSettings_.daemonEnabled =
                (SendMessageW(daemonCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
    }
}

void SettingsWindow::syncSecurityListsFromControls() {
    if (!tabControl_) {
        return;
    }
    HWND securityTab = GetDlgItem(tabControl_, IDC_TAB_SECURITY);
    if (!securityTab) {
        return;
    }
    auto toUtf8 = [](const std::vector<std::wstring>& items) {
        std::vector<std::string> out;
        out.reserve(items.size());
        for (const auto& item : items) {
            out.emplace_back(item.begin(), item.end());
        }
        return out;
    };

    currentSettings_.allowedDirectories =
        toUtf8(GetListBoxItemsW(GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_LIST)));
    currentSettings_.allowedApplications =
        toUtf8(GetListBoxItemsW(GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_LIST)));
    currentSettings_.allowedCommands =
        toUtf8(GetListBoxItemsW(GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_LIST)));
}

void SettingsWindow::updateSecurityButtonsState() {
    if (!tabControl_) {
        return;
    }
    HWND securityTab = GetDlgItem(tabControl_, IDC_TAB_SECURITY);
    if (!securityTab) {
        return;
    }

    auto updateForList = [](HWND list, HWND editBtn, HWND removeBtn, HWND upBtn, HWND downBtn) {
        if (!list) {
            return;
        }
        int index = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
        LRESULT count = SendMessageW(list, LB_GETCOUNT, 0, 0);
        bool hasSelection = index != LB_ERR;
        if (editBtn) EnableWindow(editBtn, hasSelection);
        if (removeBtn) EnableWindow(removeBtn, hasSelection);
        if (upBtn) EnableWindow(upBtn, hasSelection && index > 0);
        if (downBtn) EnableWindow(downBtn, hasSelection && count != LB_ERR && index < count - 1);
    };

    updateForList(
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_LIST),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_EDIT),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_REMOVE),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_UP),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_DIRS_DOWN));

    updateForList(
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_LIST),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_EDIT),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_REMOVE),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_UP),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_APPS_DOWN));

    updateForList(
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_LIST),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_EDIT),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_REMOVE),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_UP),
        GetDlgItem(securityTab, IDC_SECURITY_ALLOWED_COMMANDS_DOWN));
}

namespace {
BOOL CALLBACK EnumChildProc(HWND child, LPARAM lParam) {
    HFONT font = reinterpret_cast<HFONT>(lParam);
    SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}
}

void SettingsWindow::applyUIFont() {
    if (!hwnd_) {
        return;
    }
    if (!uiFont_) {
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
            uiFont_ = CreateFontIndirectW(&metrics.lfMessageFont);
        }
    }
    if (!uiFont_) {
        return;
    }

    SendMessageW(hwnd_, WM_SETFONT, (WPARAM)uiFont_, TRUE);
    if (tabControl_) {
        SendMessageW(tabControl_, WM_SETFONT, (WPARAM)uiFont_, TRUE);
    }
    EnumChildWindows(hwnd_, EnumChildProc, (LPARAM)uiFont_);
}
