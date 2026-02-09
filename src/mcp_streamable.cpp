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
#include "mcp_streamable.h"
#include "app_globals.h"
#include "mcp_handlers.h"
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <vector>
#include <windows.h>
#include "mcp/tool_registry.h"
#include "policy/policy_guard.h"
#include "support/config_manager.h"
#include "support/audit_logger.h"
#include "support/dashboard_window.h"

// ── McpSessionStore ────────────────────────────────────────

McpSessionStore& McpSessionStore::getInstance() {
    static McpSessionStore instance;
    return instance;
}

// 生成 32 字节随机十六进制字符串作为 session ID
static std::string GenerateSessionId() {
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

std::string McpSessionStore::createSession(const std::string& protocolVersion) {
    std::lock_guard<std::mutex> lock(mutex_);
    McpSession session;
    session.sessionId = GenerateSessionId();
    session.protocolVersion = protocolVersion;
    session.initialized = false;
    session.createdAt = std::time(nullptr);
    std::string id = session.sessionId;
    sessions_[id] = std::move(session);
    return id;
}

const McpSession* McpSessionStore::findSession(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) return nullptr;
    return &it->second;
}

bool McpSessionStore::markInitialized(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) return false;
    it->second.initialized = true;
    return true;
}

void McpSessionStore::removeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sessionId);
}

// ── HTTP 辅助 ──────────────────────────────────────────────

// 构建 JSON-RPC 2.0 成功响应
static nlohmann::json MakeJsonRpcResult(const nlohmann::json& id, const nlohmann::json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}

// 构建 JSON-RPC 2.0 错误响应
static nlohmann::json MakeJsonRpcError(const nlohmann::json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}}
    };
}

// 构建完整 HTTP 响应（200 OK + JSON body）
static std::string MakeHttpJsonResponse(const std::string& body,
                                         const std::string& extraHeaders = "") {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/json\r\n"
        << "Access-Control-Allow-Origin: *\r\n";
    if (!extraHeaders.empty()) {
        oss << extraHeaders;
    }
    oss << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

static std::string MakeHttpErrorResponse(int httpStatus, const std::string& statusText,
                                          const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << httpStatus << " " << statusText << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

static std::string MakeHttp202() {
    return "HTTP/1.1 202 Accepted\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Content-Length: 0\r\n"
           "\r\n";
}

static std::string MakeHttp405() {
    std::string body = "{\"error\":\"Method Not Allowed\"}";
    std::ostringstream oss;
    oss << "HTTP/1.1 405 Method Not Allowed\r\n"
        << "Content-Type: application/json\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Allow: POST, OPTIONS\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

// 从 HTTP 请求中提取指定 header（不区分大小写）
static std::string ExtractHeader(const std::string& request, const std::string& headerNameLower) {
    std::string searchKey = "\r\n" + headerNameLower + ":";
    // 转为小写进行查找
    std::string lowerReq = request;
    std::transform(lowerReq.begin(), lowerReq.end(), lowerReq.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    size_t pos = lowerReq.find(searchKey);
    if (pos == std::string::npos) return "";
    size_t valStart = pos + searchKey.size();
    // 跳过前导空格
    while (valStart < request.size() && request[valStart] == ' ') ++valStart;
    size_t valEnd = request.find("\r\n", valStart);
    if (valEnd == std::string::npos) valEnd = request.size();
    return request.substr(valStart, valEnd - valStart);
}

// 提取 HTTP body
static std::string ExtractBody(const std::string& request) {
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return request.substr(pos + 4);
}

// 提取 HTTP method
static std::string ExtractMethod(const std::string& request) {
    size_t sp = request.find(' ');
    if (sp == std::string::npos) return "";
    return request.substr(0, sp);
}

// ── JSON-RPC 错误码 ────────────────────────────────────────
static const int kParseError      = -32700;
static const int kInvalidRequest  = -32600;
static const int kMethodNotFound  = -32601;
static const int kInvalidParams   = -32602;
static const int kServerError     = -32000;

// ── 协议支持的版本 ─────────────────────────────────────────
static const char* kSupportedProtocolVersion = "2024-11-05";
static const std::vector<std::string> kAcceptedProtocolVersions = {
    "2024-11-05", "2025-03-26"
};

static bool IsAcceptedProtocolVersion(const std::string& version) {
    for (const auto& v : kAcceptedProtocolVersions) {
        if (v == version) return true;
    }
    return false;
}

// ── 核心分发 ───────────────────────────────────────────────

std::string HandleMcpStreamableHttp(const std::string& request) {
    std::string method = ExtractMethod(request);

    // GET /mcp 和 DELETE /mcp 返回 405
    if (method != "POST") {
        return MakeHttp405();
    }

    // ── Origin 检查（严格 host 匹配） ──
    std::string origin = ExtractHeader(request, "origin");
    if (!origin.empty()) {
        // 从 origin 中提取 host 部分（scheme://host[:port]）
        bool originAllowed = false;
        size_t schemeEnd = origin.find("://");
        if (schemeEnd != std::string::npos) {
            std::string hostPort = origin.substr(schemeEnd + 3);
            // 去掉 port（如果有）
            size_t colonPos = hostPort.find(':');
            std::string host = (colonPos != std::string::npos) ? hostPort.substr(0, colonPos) : hostPort;
            // 去掉尾部 path（如果有）
            size_t slashPos = host.find('/');
            if (slashPos != std::string::npos) host = host.substr(0, slashPos);

            originAllowed = (host == "localhost" || host == "127.0.0.1");
        }
        // TODO: 未来可添加 ConfigManager 的 allowed_origins 配置支持
        if (!originAllowed) {
            return MakeHttpErrorResponse(403, "Forbidden",
                MakeJsonRpcError(nullptr, kServerError, "Origin not allowed").dump());
        }
    }

    // ── MCP-Protocol-Version 检查（允许已知版本，始终以服务器支持的版本回复）──
    std::string protoVersionHeader = ExtractHeader(request, "mcp-protocol-version");
    if (!protoVersionHeader.empty() && !IsAcceptedProtocolVersion(protoVersionHeader)) {
        return MakeHttpErrorResponse(400, "Bad Request",
            MakeJsonRpcError(nullptr, kInvalidRequest,
                "Unsupported MCP protocol version: " + protoVersionHeader +
                ". Supported: 2024-11-05, 2025-03-26").dump());
    }

    // ── 解析 body ──
    std::string body = ExtractBody(request);
    nlohmann::json msg;
    try {
        msg = nlohmann::json::parse(body);
    } catch (const std::exception& e) {
        return MakeHttpJsonResponse(
            MakeJsonRpcError(nullptr, kParseError,
                std::string("Parse error: ") + e.what()).dump());
    }

    // 不支持 batch（数组）
    if (msg.is_array()) {
        return MakeHttpJsonResponse(
            MakeJsonRpcError(nullptr, kInvalidRequest,
                "Batch requests are not supported").dump());
    }

    if (!msg.is_object()) {
        return MakeHttpJsonResponse(
            MakeJsonRpcError(nullptr, kInvalidRequest, "Expected JSON object").dump());
    }

    // ── 校验 jsonrpc 字段 ──
    if (!msg.contains("jsonrpc") || msg["jsonrpc"] != "2.0") {
        return MakeHttpJsonResponse(
            MakeJsonRpcError(nullptr, kInvalidRequest,
                "Missing or invalid 'jsonrpc' field, must be '2.0'").dump());
    }

    // ── 判定消息类型 ──
    bool hasMethod = msg.contains("method") && msg["method"].is_string();
    bool hasId     = msg.contains("id");
    bool hasResult = msg.contains("result");
    bool hasError  = msg.contains("error");

    // Response（客户端发来的 response，忽略）
    if (!hasMethod && (hasResult || hasError)) {
        return MakeHttp202();
    }

    // Notification（有 method 无 id）
    if (hasMethod && !hasId) {
        std::string methodName = msg["method"].get<std::string>();

        if (methodName == "notifications/initialized") {
            std::string sid = ExtractHeader(request, "mcp-session-id");
            if (!sid.empty()) {
                McpSessionStore::getInstance().markInitialized(sid);
            }
        }
        // 所有 notification 返回 202
        return MakeHttp202();
    }

    // 不是有效的 Request（无 method）
    if (!hasMethod) {
        return MakeHttpJsonResponse(
            MakeJsonRpcError(msg.value("id", nlohmann::json(nullptr)),
                kInvalidRequest, "Missing 'method' field").dump());
    }

    // ── Request（有 method 和 id）──
    std::string methodName = msg["method"].get<std::string>();
    nlohmann::json rpcId = msg["id"];
    nlohmann::json params = msg.value("params", nlohmann::json::object());

    AppendHttpServerLogA("[MCP] RPC request: " + methodName);
    if (g_dashboard) g_dashboard->logRequest("MCP", methodName);

    // ── initialize ──
    if (methodName == "initialize") {
        std::string clientProtoVersion = params.value("protocolVersion", std::string(""));
        if (!clientProtoVersion.empty() && !IsAcceptedProtocolVersion(clientProtoVersion)) {
            return MakeHttpJsonResponse(
                MakeJsonRpcError(rpcId, kInvalidRequest,
                    "Unsupported client protocol version: " + clientProtoVersion +
                    ". Supported: 2024-11-05, 2025-03-26").dump());
        }
        AppendHttpServerLogA("[MCP] Client protocol version: " + (clientProtoVersion.empty() ? "(none)" : clientProtoVersion));

        // 创建 session（始终以服务器支持的版本回复）
        std::string sessionId = McpSessionStore::getInstance().createSession(kSupportedProtocolVersion);

        nlohmann::json result = {
            {"protocolVersion", kSupportedProtocolVersion},
            {"capabilities", {{"tools", nlohmann::json::object()}}},
            {"serverInfo", {
                {"name", "ClawDesk MCP Server"},
                {"version", CLAWDESK_VERSION}
            }}
        };

        nlohmann::json rpcResponse = MakeJsonRpcResult(rpcId, result);
        std::string responseBody = rpcResponse.dump();

        // 在 header 中返回 MCP-Session-Id
        std::string extraHeaders = "MCP-Session-Id: " + sessionId + "\r\n";
        if (g_dashboard) g_dashboard->logSuccess("MCP", "initialize OK, session=" + sessionId);
        return MakeHttpJsonResponse(responseBody, extraHeaders);
    }

    // ── 非 initialize 方法需要有效 session ──
    std::string sessionId = ExtractHeader(request, "mcp-session-id");
    if (sessionId.empty()) {
        return MakeHttpErrorResponse(400, "Bad Request",
            MakeJsonRpcError(rpcId, kInvalidRequest,
                "Missing MCP-Session-Id header").dump());
    }

    const McpSession* session = McpSessionStore::getInstance().findSession(sessionId);
    if (!session) {
        return MakeHttpErrorResponse(404, "Not Found",
            MakeJsonRpcError(rpcId, kInvalidRequest,
                "Unknown or expired session. Please re-initialize.").dump());
    }

    // ── ping ──
    if (methodName == "ping") {
        return MakeHttpJsonResponse(
            MakeJsonRpcResult(rpcId, nlohmann::json::object()).dump());
    }

    // ── tools/list ──
    if (methodName == "tools/list") {
        auto tools = ToolRegistry::getInstance().getAllTools();
        nlohmann::json toolsArray = nlohmann::json::array();
        for (const auto& tool : tools) {
            toolsArray.push_back({
                {"name", tool.name},
                {"description", tool.description},
                {"inputSchema", tool.inputSchema}
            });
        }
        nlohmann::json result = {{"tools", toolsArray}};
        if (g_dashboard) g_dashboard->logSuccess("MCP", "tools/list: " + std::to_string(tools.size()) + " tools");
        return MakeHttpJsonResponse(MakeJsonRpcResult(rpcId, result).dump());
    }

    // ── tools/call ──
    if (methodName == "tools/call") {
        if (!params.contains("name") || !params["name"].is_string()) {
            return MakeHttpJsonResponse(
                MakeJsonRpcError(rpcId, kInvalidParams,
                    "Missing or invalid 'name' in params").dump());
        }

        std::string toolName = params["name"].get<std::string>();
        nlohmann::json args = params.value("arguments", nlohmann::json::object());

        if (!ToolRegistry::getInstance().hasTool(toolName)) {
            return MakeHttpJsonResponse(
                MakeJsonRpcError(rpcId, kMethodNotFound,
                    "Unknown tool: " + toolName).dump());
        }

        // PolicyGuard 检查
        if (g_policyGuard) {
            auto decision = g_policyGuard->evaluateToolCall(toolName, args);
            if (!decision.allowed) {
                return MakeHttpJsonResponse(
                    MakeJsonRpcError(rpcId, kServerError,
                        "Policy denied: " + decision.reason).dump());
            }
        }

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
            if (g_dashboard) g_dashboard->logProcessing("MCP", "tools/call: " + toolName);
            auto tool = ToolRegistry::getInstance().getTool(toolName);
            nlohmann::json toolResult = tool.handler(args);
            if (g_dashboard) g_dashboard->logSuccess("MCP", "tools/call OK: " + toolName);
            return MakeHttpJsonResponse(MakeJsonRpcResult(rpcId, toolResult).dump());
        } catch (const std::exception& e) {
            if (g_dashboard) g_dashboard->logError("MCP", "tools/call error: " + toolName + " - " + e.what());
            return MakeHttpJsonResponse(
                MakeJsonRpcError(rpcId, kServerError,
                    std::string("Tool execution error: ") + e.what()).dump());
        }
    }

    // ── 未知方法 ──
    return MakeHttpJsonResponse(
        MakeJsonRpcError(rpcId, kMethodNotFound,
            "Method not found: " + methodName).dump());
}
