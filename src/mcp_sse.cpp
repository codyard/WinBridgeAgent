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
#include "mcp_sse.h"
#include "http_server.h"
#include "app_globals.h"
#include "mcp_handlers.h"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "mcp/tool_registry.h"
#include "policy/policy_guard.h"
#include "support/config_manager.h"
#include "support/audit_logger.h"
#include "support/dashboard_window.h"

// ── 生成 32 字节随机十六进制 session ID ────────────────────
static std::string GenerateSseSessionId() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 15);
    const char hex[] = "0123456789abcdef";
    std::string id;
    id.reserve(32);
    for (int i = 0; i < 32; ++i) {
        id += hex[dist(rng)];
    }
    return id;
}

// ── SseSessionStore ────────────────────────────────────────

SseSessionStore& SseSessionStore::getInstance() {
    static SseSessionStore instance;
    return instance;
}

static const size_t kMaxSseSessions = 16;       // 最多同时 16 个 SSE 连接
static const DWORD  kSseSessionTtlMs = 3600000;  // 1 小时 TTL

static void CloseSessionSocket(const std::shared_ptr<SseSession>& session) {
    if (!session) return;
    std::lock_guard<std::mutex> lock(session->writeMutex);
    if (session->socket != INVALID_SOCKET) {
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
    }
}

std::shared_ptr<SseSession> SseSessionStore::createSession(SOCKET socket) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 清理过期 session
    DWORD now = GetTickCount();
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if ((now - it->second->createdAt) > kSseSessionTtlMs || !it->second->alive.load()) {
            it->second->alive.store(false);
            CloseSessionSocket(it->second);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }

    // 拒绝超上限
    if (sessions_.size() >= kMaxSseSessions) {
        return nullptr;
    }

    auto session = std::make_shared<SseSession>();
    session->sessionId = GenerateSseSessionId();
    session->socket = socket;
    session->alive.store(true);
    session->initialized = false;
    session->createdAt = GetTickCount();
    sessions_[session->sessionId] = session;
    return session;
}

std::shared_ptr<SseSession> SseSessionStore::findSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) return nullptr;
    return it->second;
}

void SseSessionStore::removeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sessionId);
}

void SseSessionStore::shutdownAllSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : sessions_) {
        auto& session = pair.second;
        session->alive.store(false);
        // 关闭 socket 会让阻塞在 Sleep/send 的线程尽快收到错误并退出
        CloseSessionSocket(session);
    }
    sessions_.clear();
}

size_t SseSessionStore::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

bool SseSessionStore::sendSseEvent(const std::string& sessionId,
                                    const std::string& eventType,
                                    const std::string& data) {
    auto session = findSession(sessionId);
    if (!session || !session->alive.load()) return false;

    // 构建 SSE 帧：event 和 data 字段各占一行，以空行结尾
    std::string frame;
    if (!eventType.empty()) {
        frame += "event: " + eventType + "\n";
    }
    // data 可以是多行，每行加 "data: " 前缀
    // 但 JSON 通常是单行
    frame += "data: " + data + "\n\n";

    std::lock_guard<std::mutex> lock(session->writeMutex);
    if (session->socket == INVALID_SOCKET) {
        session->alive.store(false);
        return false;
    }
    if (!SendAll(session->socket, frame)) {
        session->alive.store(false);
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
        return false;
    }
    return true;
}

// ── 辅助函数 ──────────────────────────────────────────────

// 从 request 中提取 query parameter
static std::string GetSseQueryParam(const std::string& request, const std::string& paramName) {
    // 解析请求行中的 URL
    size_t firstSpace = request.find(' ');
    if (firstSpace == std::string::npos) return "";
    size_t secondSpace = request.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos) return "";
    std::string url = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    size_t qmark = url.find('?');
    if (qmark == std::string::npos) return "";
    std::string query = url.substr(qmark + 1);

    std::string key = paramName + "=";
    size_t pos = query.find(key);
    if (pos == std::string::npos) return "";
    size_t valStart = pos + key.size();
    size_t valEnd = query.find('&', valStart);
    if (valEnd == std::string::npos) valEnd = query.size();
    return query.substr(valStart, valEnd - valStart);
}

// 从 HTTP 请求中提取 body
static std::string ExtractSseBody(const std::string& request) {
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return request.substr(pos + 4);
}

// 从 HTTP 请求中提取 header（不区分大小写）
static std::string ExtractSseHeader(const std::string& request, const std::string& headerNameLower) {
    std::string searchKey = "\r\n" + headerNameLower + ":";
    std::string lowerReq = request;
    std::transform(lowerReq.begin(), lowerReq.end(), lowerReq.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    size_t pos = lowerReq.find(searchKey);
    if (pos == std::string::npos) return "";
    size_t valStart = pos + searchKey.size();
    while (valStart < request.size() && request[valStart] == ' ') ++valStart;
    size_t valEnd = request.find("\r\n", valStart);
    if (valEnd == std::string::npos) valEnd = request.size();
    return request.substr(valStart, valEnd - valStart);
}

// JSON-RPC 错误码
static const int kParseError     = -32700;
static const int kInvalidRequest = -32600;
static const int kMethodNotFound = -32601;
static const int kInvalidParams  = -32602;
static const int kServerError    = -32000;

static const char* kSupportedProtocolVersion = "2024-11-05";

// 构建 JSON-RPC 成功响应
static nlohmann::json MakeRpcResult(const nlohmann::json& id, const nlohmann::json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

// 构建 JSON-RPC 错误响应
static nlohmann::json MakeRpcError(const nlohmann::json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

// ── 检测 SSE 请求 ─────────────────────────────────────────

bool IsSseRequest(const std::string& request) {
    // 匹配 "GET /sse " 或 "GET /sse?"
    if (request.size() < 9) return false;
    if (request.substr(0, 9) == "GET /sse " || request.substr(0, 9) == "GET /sse?") return true;
    if (request.size() >= 10 && request.substr(0, 10) == "GET /sse H") return true;
    return false;
}

// ── SSE 连接线程 ──────────────────────────────────────────

DWORD WINAPI SseConnectionThread(LPVOID lpParam) {
    SseThreadParams* params = static_cast<SseThreadParams*>(lpParam);
    SOCKET sock = params->socket;
    std::string request = std::move(params->request);
    delete params;

    AppendHttpServerLogA("[SSE] New SSE connection");
    if (g_dashboard) g_dashboard->logRequest("SSE", "GET /sse - new connection");

    // 设置 SSE socket 发送超时（5 秒），防止阻塞
    int sndTimeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sndTimeout, sizeof(sndTimeout));

    // 授权检查
    if (!IsAuthorizedRequest(request)) {
        std::string resp = MakeUnauthorizedResponse();
        SendAll(sock, resp);
        closesocket(sock);
        AppendHttpServerLogA("[SSE] Unauthorized, closing");
        return 0;
    }

    // 创建 session（可能因上限被拒绝）
    auto session = SseSessionStore::getInstance().createSession(sock);
    if (!session) {
        std::string body503 = "{\"error\":\"Too many SSE sessions, try later\"}";
        std::string resp = "HTTP/1.1 503 Service Unavailable\r\n"
                           "Content-Type: application/json\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Length: " + std::to_string(body503.size()) + "\r\n"
                           "\r\n" + body503;
        SendAll(sock, resp);
        closesocket(sock);
        AppendHttpServerLogA("[SSE] Session limit reached, rejecting");
        if (g_dashboard) g_dashboard->logError("SSE", "Session limit reached");
        return 0;
    }
    std::string sessionId = session->sessionId;
    AppendHttpServerLogA("[SSE] Session created: " + sessionId);
    if (g_dashboard) g_dashboard->logSuccess("SSE", "Session created: " + sessionId);

    // 发送 SSE 响应头
    std::string sseHeaders =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";

    {
        bool headerOk = false;
        {
            std::lock_guard<std::mutex> lock(session->writeMutex);
            headerOk = SendAll(sock, sseHeaders);
        }
        if (!headerOk) {
            AppendHttpServerLogA("[SSE] Failed to send headers, closing");
            SseSessionStore::getInstance().removeSession(sessionId);
            CloseSessionSocket(session);
            return 0;
        }
    }

    // 发送 endpoint 事件，告诉客户端 POST 地址（使用绝对 URL）
    std::string host = ExtractSseHeader(request, "host");
    std::string endpointData;
    if (!host.empty()) {
        endpointData = "http://" + host + "/messages?sessionId=" + sessionId;
    } else {
        // fallback: 用配置的监听地址和端口
        int port = g_configManager ? g_configManager->getServerPort() : 35182;
        endpointData = "http://127.0.0.1:" + std::to_string(port) + "/messages?sessionId=" + sessionId;
    }
    if (!SseSessionStore::getInstance().sendSseEvent(sessionId, "endpoint", endpointData)) {
        AppendHttpServerLogA("[SSE] Failed to send endpoint event, closing");
        SseSessionStore::getInstance().removeSession(sessionId);
        CloseSessionSocket(session);
        return 0;
    }

    AppendHttpServerLogA("[SSE] Endpoint event sent: " + endpointData);

    // Keep-alive 循环：定期发送 SSE comment 以检测断连
    // 用短间隔 Sleep 代替一次 15 秒长等待，加速退出响应
    while (g_running && session->alive.load()) {
        for (int i = 0; i < 15 && g_running && session->alive.load(); ++i) {
            Sleep(1000);
        }

        if (!g_running || !session->alive.load()) break;

        // SSE comment（以 ":" 开头）不会被客户端当作事件
        std::string ping = ": ping\n\n";
        {
            std::lock_guard<std::mutex> lock(session->writeMutex);
            SOCKET s = session->socket;
            if (s == INVALID_SOCKET || !SendAll(s, ping)) {
                AppendHttpServerLogA("[SSE] Ping failed, client disconnected");
                if (g_dashboard) g_dashboard->logError("SSE", "Client disconnected: " + sessionId);
                session->alive.store(false);
                if (session->socket != INVALID_SOCKET) {
                    closesocket(session->socket);
                    session->socket = INVALID_SOCKET;
                }
                break;
            }
        }
    }

    // 清理（socket 可能已被 shutdownAllSessions 关闭）
    AppendHttpServerLogA("[SSE] Session closing: " + sessionId);
    if (g_dashboard) g_dashboard->logProcessing("SSE", "Session closing: " + sessionId);
    SseSessionStore::getInstance().removeSession(sessionId);
    CloseSessionSocket(session);
    return 0;
}

// ── 处理 POST /messages ──────────────────────────────────

// 校验 sessionId 格式：32 个十六进制字符
static bool IsValidSessionId(const std::string& id) {
    if (id.size() != 32) return false;
    for (char c : id) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

std::string HandleSseMessage(const std::string& request) {
    // 提取并校验 sessionId
    std::string sessionId = GetSseQueryParam(request, "sessionId");
    if (sessionId.empty() || !IsValidSessionId(sessionId)) {
        nlohmann::json err = MakeRpcError(nullptr, kInvalidRequest, "Missing sessionId query parameter");
        std::string body = err.dump();
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n"
               "\r\n" + body;
    }

    // 查找 session
    auto session = SseSessionStore::getInstance().findSession(sessionId);
    if (!session || !session->alive.load()) {
        nlohmann::json err = MakeRpcError(nullptr, kInvalidRequest,
            "Unknown or expired session. Connect to GET /sse first.");
        std::string body = err.dump();
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n"
               "\r\n" + body;
    }

    // 解析 body
    std::string rawBody = ExtractSseBody(request);
    nlohmann::json msg;
    try {
        msg = nlohmann::json::parse(rawBody);
    } catch (const std::exception& e) {
        nlohmann::json err = MakeRpcError(nullptr, kParseError,
            std::string("Parse error: ") + e.what());
        std::string body = err.dump();
        SseSessionStore::getInstance().sendSseEvent(sessionId, "message", body);
        return "HTTP/1.1 202 Accepted\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }

    if (!msg.is_object()) {
        nlohmann::json err = MakeRpcError(nullptr, kInvalidRequest, "Expected JSON object");
        std::string body = err.dump();
        SseSessionStore::getInstance().sendSseEvent(sessionId, "message", body);
        return "HTTP/1.1 202 Accepted\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }

    // 校验 jsonrpc
    if (!msg.contains("jsonrpc") || msg["jsonrpc"] != "2.0") {
        nlohmann::json err = MakeRpcError(nullptr, kInvalidRequest,
            "Missing or invalid 'jsonrpc' field, must be '2.0'");
        std::string body = err.dump();
        SseSessionStore::getInstance().sendSseEvent(sessionId, "message", body);
        return "HTTP/1.1 202 Accepted\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }

    bool hasMethod = msg.contains("method") && msg["method"].is_string();
    bool hasId     = msg.contains("id");

    // Notification（有 method 无 id）
    if (hasMethod && !hasId) {
        std::string methodName = msg["method"].get<std::string>();
        if (methodName == "notifications/initialized") {
            session->initialized = true;
            AppendHttpServerLogA("[SSE] Session initialized: " + sessionId);
        }
        // notification 返回 202，不通过 SSE 发送响应
        return "HTTP/1.1 202 Accepted\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }

    if (!hasMethod) {
        nlohmann::json err = MakeRpcError(msg.value("id", nlohmann::json(nullptr)),
            kInvalidRequest, "Missing 'method' field");
        std::string body = err.dump();
        SseSessionStore::getInstance().sendSseEvent(sessionId, "message", body);
        return "HTTP/1.1 202 Accepted\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }

    // ── Request（有 method 和 id）──
    std::string methodName = msg["method"].get<std::string>();
    nlohmann::json rpcId = msg["id"];
    nlohmann::json params = msg.value("params", nlohmann::json::object());

    AppendHttpServerLogA("[SSE] RPC request: " + methodName);
    if (g_dashboard) g_dashboard->logRequest("SSE", methodName);

    nlohmann::json rpcResponse;

    // ── initialize ──
    if (methodName == "initialize") {
        session->protocolVersion = kSupportedProtocolVersion;
        nlohmann::json result = {
            {"protocolVersion", kSupportedProtocolVersion},
            {"capabilities", {{"tools", nlohmann::json::object()}}},
            {"serverInfo", {
                {"name", "ClawDesk MCP Server"},
                {"version", CLAWDESK_VERSION}
            }}
        };
        rpcResponse = MakeRpcResult(rpcId, result);
        if (g_dashboard) g_dashboard->logSuccess("SSE", "initialize OK, version=" + std::string(kSupportedProtocolVersion));
    }
    // ── ping ──
    else if (methodName == "ping") {
        rpcResponse = MakeRpcResult(rpcId, nlohmann::json::object());
    }
    // ── tools/list ──
    else if (methodName == "tools/list") {
        auto tools = ToolRegistry::getInstance().getAllTools();
        nlohmann::json toolsArray = nlohmann::json::array();
        for (const auto& tool : tools) {
            toolsArray.push_back({
                {"name", tool.name},
                {"description", tool.description},
                {"inputSchema", tool.inputSchema}
            });
        }
        rpcResponse = MakeRpcResult(rpcId, {{"tools", toolsArray}});
        if (g_dashboard) g_dashboard->logSuccess("SSE", "tools/list: " + std::to_string(tools.size()) + " tools");
    }
    // ── tools/call ──
    else if (methodName == "tools/call") {
        if (!params.contains("name") || !params["name"].is_string()) {
            rpcResponse = MakeRpcError(rpcId, kInvalidParams,
                "Missing or invalid 'name' in params");
        } else {
            std::string toolName = params["name"].get<std::string>();
            nlohmann::json args = params.value("arguments", nlohmann::json::object());

            if (!ToolRegistry::getInstance().hasTool(toolName)) {
                rpcResponse = MakeRpcError(rpcId, kMethodNotFound,
                    "Unknown tool: " + toolName);
            } else {
                // PolicyGuard 检查
                if (g_policyGuard) {
                    auto decision = g_policyGuard->evaluateToolCall(toolName, args);
                    if (!decision.allowed) {
                        rpcResponse = MakeRpcError(rpcId, kServerError,
                            "Policy denied: " + decision.reason);
                    }
                }

                // 如果还没被 policy 拒绝
                if (rpcResponse.is_null()) {
                    // 审计日志
                    if (g_auditLogger) {
                        clawdesk::AuditLogEntry entry;
                        entry.time = g_auditLogger->getCurrentTimestamp();
                        entry.tool = toolName;
                        entry.risk = ToolRegistry::getInstance().getTool(toolName).riskLevel;
                        entry.details = args;
                        entry.result = "executing";
                        g_auditLogger->logToolCall(entry);
                    }

                    try {
                        if (g_dashboard) g_dashboard->logProcessing("SSE", "tools/call: " + toolName);
                        auto tool = ToolRegistry::getInstance().getTool(toolName);
                        nlohmann::json toolResult = tool.handler(args);
                        rpcResponse = MakeRpcResult(rpcId, toolResult);
                        if (g_dashboard) g_dashboard->logSuccess("SSE", "tools/call OK: " + toolName);
                    } catch (const std::exception& e) {
                        rpcResponse = MakeRpcError(rpcId, kServerError,
                            std::string("Tool execution error: ") + e.what());
                        if (g_dashboard) g_dashboard->logError("SSE", "tools/call error: " + toolName + " - " + e.what());
                    }
                }
            }
        }
    }
    // ── 未知方法 ──
    else {
        rpcResponse = MakeRpcError(rpcId, kMethodNotFound,
            "Method not found: " + methodName);
    }

    // 通过 SSE 发送 JSON-RPC 响应
    std::string responseData = rpcResponse.dump();
    SseSessionStore::getInstance().sendSseEvent(sessionId, "message", responseData);

    // POST 返回 202 Accepted
    return "HTTP/1.1 202 Accepted\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Content-Length: 0\r\n"
           "\r\n";
}
