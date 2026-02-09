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
#ifndef CLAWDESK_WINDOW_SERVICE_H
#define CLAWDESK_WINDOW_SERVICE_H

#include <string>
#include <vector>
#include <windows.h>

struct WindowInfo {
    std::string title;
    std::string processName;
    DWORD pid;
};

class WindowService {
public:
    std::vector<WindowInfo> listVisibleWindows();
    bool focusWindow(const std::string& title,
                     const std::string& processName,
                     WindowInfo* outInfo);
    bool findWindow(const std::string& title,
                    const std::string& processName,
                    DWORD pid,
                    HWND* outHwnd,
                    WindowInfo* outInfo);

private:
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
    static BOOL CALLBACK enumFocusProc(HWND hwnd, LPARAM lParam);
    static BOOL CALLBACK enumFindProc(HWND hwnd, LPARAM lParam);
    bool isSystemWindow(HWND hwnd);
    std::string getWindowTitle(HWND hwnd);
    std::string getProcessName(DWORD pid);
};

#endif // CLAWDESK_WINDOW_SERVICE_H
