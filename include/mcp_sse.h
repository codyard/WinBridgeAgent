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
#ifndef CLAWDESK_MCP_SSE_H
#define CLAWDESK_MCP_SSE_H

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>

// ── SSE Session ────────────────────────────────────────────
struct SseSession {
    std::string sessionId;
    SOCKET      socket;
    std::mutex  writeMutex;      // 保护 socket 写操作
    std::atomic_bool alive;      // SSE 连接是否存活（跨线程读写）
    // MCP 协议状态
    std::string protocolVersion;
    bool        initialized;     // notifications/initialized 已收到
    DWORD       createdAt;       // GetTickCount() 创建时间

    SseSession() : socket(INVALID_SOCKET), alive(false), initialized(false), createdAt(0) {}
};

// SSE Session 全局存储（线程安全）
class SseSessionStore {
public:
    static SseSessionStore& getInstance();

    // 创建 session，返回 shared_ptr；超过上限或 TTL 淘汰时返回 nullptr
    // socket 生命周期由 store 统一管理（send/TTL/shutdown 均会 close）
    std::shared_ptr<SseSession> createSession(SOCKET socket);

    // 查找 session
    std::shared_ptr<SseSession> findSession(const std::string& sessionId);

    // 移除 session
    void removeSession(const std::string& sessionId);

    // 关闭所有 session（退出时调用，关闭 socket 让线程尽快退出）
    void shutdownAllSessions();

    // 当前活跃 session 数量
    size_t sessionCount() const;

    // 向 SSE 客户端发送事件（线程安全）
    bool sendSseEvent(const std::string& sessionId,
                      const std::string& eventType,
                      const std::string& data);

private:
    SseSessionStore() = default;
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<SseSession>> sessions_;
};

// ── SSE 连接线程参数 ──────────────────────────────────────
struct SseThreadParams {
    SOCKET      socket;
    std::string request;  // 原始 HTTP 请求（用于授权检查）
};

// SSE 连接线程入口
DWORD WINAPI SseConnectionThread(LPVOID lpParam);

// 处理 POST /messages 请求，返回完整 HTTP 响应
std::string HandleSseMessage(const std::string& request);

// 检查是否为 SSE 请求（GET /sse）
bool IsSseRequest(const std::string& request);

#endif // CLAWDESK_MCP_SSE_H
