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
#ifndef CLAWDESK_MCP_HANDLERS_H
#define CLAWDESK_MCP_HANDLERS_H

#include <string>
#include <nlohmann/json.hpp>

// MCP 工具辅助
nlohmann::json MakeTextContent(const std::string& text, bool isError = false);
std::string    DumpMcpResponse(const nlohmann::json& response);

// MCP 工具注册（启动时调用一次）
void RegisterMcpTools();

// MCP 协议 handlers
std::string HandleMCPInitialize(const std::string& body);
std::string HandleMCPToolsList();
std::string HandleMCPToolsCall(const std::string& body);

#endif // CLAWDESK_MCP_HANDLERS_H
