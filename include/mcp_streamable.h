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
#ifndef CLAWDESK_MCP_STREAMABLE_H
#define CLAWDESK_MCP_STREAMABLE_H

#include <string>
#include <map>
#include <mutex>
#include <ctime>

// ── MCP Session ────────────────────────────────────────────
struct McpSession {
    std::string sessionId;
    std::string protocolVersion;
    bool        initialized = false;   // notifications/initialized 已收到
    std::time_t createdAt   = 0;
};

// 全局 session 存储（线程安全）
class McpSessionStore {
public:
    static McpSessionStore& getInstance();

    // 创建新 session，返回 sessionId
    std::string createSession(const std::string& protocolVersion);

    // 查找 session，不存在返回 nullptr
    const McpSession* findSession(const std::string& sessionId) const;

    // 标记 session 为 initialized
    bool markInitialized(const std::string& sessionId);

    // 移除 session
    void removeSession(const std::string& sessionId);

private:
    McpSessionStore() = default;
    mutable std::mutex mutex_;
    std::map<std::string, McpSession> sessions_;
};

// ── Streamable HTTP handler ────────────────────────────────
// 处理 POST /mcp 请求，返回完整 HTTP 响应字符串
std::string HandleMcpStreamableHttp(const std::string& request);

#endif // CLAWDESK_MCP_STREAMABLE_H
