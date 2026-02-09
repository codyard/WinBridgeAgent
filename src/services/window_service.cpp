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
#include "services/window_service.h"
#include <psapi.h>
#include <vector>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "psapi.lib")

struct WindowEnumContext {
    WindowService* service;
    std::vector<WindowInfo>* results;
};

struct WindowFocusContext {
    WindowService* service;
    std::string title;
    std::string processName;
    HWND exactMatch = NULL;
    HWND partialMatch = NULL;
    WindowInfo exactInfo{};
    WindowInfo partialInfo{};
};

struct WindowFindContext {
    WindowService* service;
    std::string title;
    std::string processName;
    DWORD pid = 0;
    HWND exactMatch = NULL;
    HWND partialMatch = NULL;
    WindowInfo exactInfo{};
    WindowInfo partialInfo{};
};

namespace {
std::string ToLower(const std::string& input) {
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool MatchesProcess(const std::string& processName, const std::string& target) {
    if (target.empty()) {
        return true;
    }
    std::string proc = ToLower(processName);
    std::string tgt = ToLower(target);
    if (proc == tgt) {
        return true;
    }
    if (tgt.size() > 4 && tgt.substr(tgt.size() - 4) == ".exe") {
        tgt = tgt.substr(0, tgt.size() - 4);
    }
    if (proc.size() > 4 && proc.substr(proc.size() - 4) == ".exe") {
        proc = proc.substr(0, proc.size() - 4);
    }
    return proc == tgt;
}

bool MatchesPid(DWORD pid, DWORD target) {
    if (target == 0) {
        return true;
    }
    return pid == target;
}

bool MatchesTitle(const std::string& title, const std::string& target, bool* exact) {
    if (target.empty()) {
        if (exact) {
            *exact = false;
        }
        return true;
    }
    std::string t = ToLower(title);
    std::string tgt = ToLower(target);
    if (t == tgt) {
        if (exact) {
            *exact = true;
        }
        return true;
    }
    if (exact) {
        *exact = false;
    }
    return t.find(tgt) != std::string::npos;
}

bool BringToFront(HWND hwnd) {
    if (!IsWindow(hwnd)) {
        return false;
    }
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }
    SetForegroundWindow(hwnd);
    if (GetForegroundWindow() == hwnd) {
        return true;
    }
    HWND foreground = GetForegroundWindow();
    DWORD currentThread = GetCurrentThreadId();
    DWORD fgThread = foreground ? GetWindowThreadProcessId(foreground, NULL) : 0;
    if (fgThread != 0 && fgThread != currentThread) {
        AttachThreadInput(currentThread, fgThread, TRUE);
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        AttachThreadInput(currentThread, fgThread, FALSE);
    } else {
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
    }
    return GetForegroundWindow() == hwnd;
}
} // namespace

std::vector<WindowInfo> WindowService::listVisibleWindows() {
    std::vector<WindowInfo> results;
    WindowEnumContext context{this, &results};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&context));
    return results;
}

BOOL CALLBACK WindowService::enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* context = reinterpret_cast<WindowEnumContext*>(lParam);
    if (!context || !context->service || !context->results) {
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    if (context->service->isSystemWindow(hwnd)) {
        return TRUE;
    }

    std::string title = context->service->getWindowTitle(hwnd);
    if (title.empty()) {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    std::string processName = context->service->getProcessName(pid);

    context->results->push_back({title, processName, pid});
    return TRUE;
}

BOOL CALLBACK WindowService::enumFocusProc(HWND hwnd, LPARAM lParam) {
    auto* context = reinterpret_cast<WindowFocusContext*>(lParam);
    if (!context || !context->service) {
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    if (context->service->isSystemWindow(hwnd)) {
        return TRUE;
    }

    std::string title = context->service->getWindowTitle(hwnd);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    std::string processName = context->service->getProcessName(pid);

    if (!MatchesProcess(processName, context->processName)) {
        return TRUE;
    }

    bool exactTitle = false;
    if (!MatchesTitle(title, context->title, &exactTitle)) {
        return TRUE;
    }

    WindowInfo info{title, processName, pid};
    if (exactTitle) {
        context->exactMatch = hwnd;
        context->exactInfo = info;
        return FALSE;
    }
    if (!context->partialMatch) {
        context->partialMatch = hwnd;
        context->partialInfo = info;
    }
    return TRUE;
}

BOOL CALLBACK WindowService::enumFindProc(HWND hwnd, LPARAM lParam) {
    auto* context = reinterpret_cast<WindowFindContext*>(lParam);
    if (!context || !context->service) {
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    if (context->service->isSystemWindow(hwnd)) {
        return TRUE;
    }

    std::string title = context->service->getWindowTitle(hwnd);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    std::string processName = context->service->getProcessName(pid);

    if (!MatchesPid(pid, context->pid)) {
        return TRUE;
    }
    if (!MatchesProcess(processName, context->processName)) {
        return TRUE;
    }

    bool exactTitle = false;
    if (!MatchesTitle(title, context->title, &exactTitle)) {
        return TRUE;
    }

    WindowInfo info{title, processName, pid};
    if (exactTitle) {
        context->exactMatch = hwnd;
        context->exactInfo = info;
        return FALSE;
    }
    if (!context->partialMatch) {
        context->partialMatch = hwnd;
        context->partialInfo = info;
    }
    return TRUE;
}

bool WindowService::isSystemWindow(HWND hwnd) {
    char className[256] = {0};
    GetClassNameA(hwnd, className, sizeof(className));
    std::string cls = className;
    return cls == "Shell_TrayWnd" || cls == "Progman";
}

std::string WindowService::getWindowTitle(HWND hwnd) {
    char title[512] = {0};
    GetWindowTextA(hwnd, title, sizeof(title));
    return std::string(title);
}

std::string WindowService::getProcessName(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        return "";
    }

    char name[MAX_PATH] = {0};
    if (GetModuleBaseNameA(hProcess, NULL, name, MAX_PATH) == 0) {
        CloseHandle(hProcess);
        return "";
    }

    CloseHandle(hProcess);
    return std::string(name);
}

bool WindowService::focusWindow(const std::string& title,
                                const std::string& processName,
                                WindowInfo* outInfo) {
    HWND hwnd = NULL;
    WindowInfo info{};
    if (!findWindow(title, processName, 0, &hwnd, &info)) {
        return false;
    }
    if (outInfo) {
        *outInfo = info;
    }
    return BringToFront(hwnd);
}

bool WindowService::findWindow(const std::string& title,
                               const std::string& processName,
                               DWORD pid,
                               HWND* outHwnd,
                               WindowInfo* outInfo) {
    WindowFindContext context{this, title, processName, pid};
    EnumWindows(enumFindProc, reinterpret_cast<LPARAM>(&context));
    HWND target = context.exactMatch ? context.exactMatch : context.partialMatch;
    if (!target) {
        return false;
    }
    if (outHwnd) {
        *outHwnd = target;
    }
    if (outInfo) {
        *outInfo = context.exactMatch ? context.exactInfo : context.partialInfo;
    }
    return true;
}
