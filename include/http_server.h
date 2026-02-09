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
#ifndef CLAWDESK_HTTP_SERVER_H
#define CLAWDESK_HTTP_SERVER_H

#include <winsock2.h>
#include <windows.h>
#include <string>

// HTTP 服务器线程
DWORD WINAPI HttpServerThread(LPVOID lpParam);

// 完整接收 HTTP 请求（处理分片）
std::string RecvFullHttpRequest(SOCKET sock);

// 可靠发送：循环处理 partial send，失败返回 false
bool SendAll(SOCKET sock, const char* buf, int len);
bool SendAll(SOCKET sock, const std::string& data);

// 网络辅助
std::string GetLocalIPAddress();
bool AddFirewallRule();
bool CheckFirewallRule();

#endif // CLAWDESK_HTTP_SERVER_H
