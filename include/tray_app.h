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
#ifndef CLAWDESK_TRAY_APP_H
#define CLAWDESK_TRAY_APP_H

#include <winsock2.h> // must be included before windows.h
#include <windows.h>
#include <shellapi.h>

// 托盘窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 托盘图标管理
bool CreateTrayIcon(HWND hwnd);
void UpdateTrayIconTooltip();

#endif // CLAWDESK_TRAY_APP_H
