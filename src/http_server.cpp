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
#include "http_server.h"
#include "http_routes.h"
#include "mcp_sse.h"
#include "app_globals.h"
#include <ws2tcpip.h>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>
#include "support/config_manager.h"
#include "support/audit_logger.h"
#include "support/rate_limiter.h"
#include "policy/policy_guard.h"
#include "utils/log_path.h"

static const size_t kMaxHttpHeaderSize = 16 * 1024;       // 16 KB header 上限
static const size_t kMaxHttpBodySize   = 4 * 1024 * 1024; // 4 MB body 上限
static const int    kRecvTimeoutMs     = 30000;            // 30 秒总超时

// ── SendAll: 循环处理 partial send ────────────────────────
bool SendAll(SOCKET sock, const char* buf, int len) {
    int totalSent = 0;
    while (totalSent < len) {
        int sent = send(sock, buf + totalSent, len - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool SendAll(SOCKET sock, const std::string& data) {
    return SendAll(sock, data.c_str(), static_cast<int>(data.size()));
}

std::string RecvFullHttpRequest(SOCKET sock) {
    // 设置 socket 接收超时
    int timeout = kRecvTimeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    std::string buf;
    buf.reserve(4096);
    char tmp[4096];

    // 阶段 1：读取至少到 header 结束（\r\n\r\n）
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        int n = recv(sock, tmp, sizeof(tmp), 0);
        if (n <= 0) {
            // 连接关闭或出错
            return buf.empty() ? std::string() : buf;
        }
        buf.append(tmp, n);
        headerEnd = buf.find("\r\n\r\n");
        if (buf.size() > kMaxHttpHeaderSize && headerEnd == std::string::npos) {
            AppendHttpServerLogA("[RecvFullHttpRequest] header too large, dropping");
            return std::string();
        }
    }

    // 阶段 2：解析 Content-Length，继续读取 body
    size_t bodyOffset = headerEnd + 4;
    size_t contentLength = 0;

    // 在 header 部分查找 Content-Length（大小写不敏感）
    std::string headerPart = buf.substr(0, headerEnd);
    std::string headerLower = headerPart;
    std::transform(headerLower.begin(), headerLower.end(), headerLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    size_t clPos = headerLower.find("content-length:");
    if (clPos != std::string::npos) {
        size_t valStart = clPos + 15; // strlen("content-length:")
        size_t valEnd = headerLower.find("\r\n", valStart);
        if (valEnd == std::string::npos) valEnd = headerLower.size();
        std::string valStr = headerPart.substr(valStart, valEnd - valStart);
        // safe trim（空串或全空格时跳过，避免 npos+1 溢出）
        size_t first = valStr.find_first_not_of(" \t");
        if (first != std::string::npos) {
            size_t last = valStr.find_last_not_of(" \t");
            valStr = valStr.substr(first, last - first + 1);
            contentLength = static_cast<size_t>(std::strtoull(valStr.c_str(), nullptr, 10));
        }
    }

    if (contentLength > kMaxHttpBodySize) {
        AppendHttpServerLogA("[RecvFullHttpRequest] body too large: " + std::to_string(contentLength));
        return std::string();
    }

    // 如果没有 body 或已经收完，直接返回
    size_t totalNeeded = bodyOffset + contentLength;
    while (buf.size() < totalNeeded) {
        int n = recv(sock, tmp, sizeof(tmp), 0);
        if (n <= 0) {
            break; // 连接关闭，返回已有数据
        }
        buf.append(tmp, n);
        if (buf.size() > bodyOffset + kMaxHttpBodySize) {
            AppendHttpServerLogA("[RecvFullHttpRequest] body exceeded max during recv");
            break;
        }
    }

    return buf;
}

// HTTP 服务器线程函数
// 辅助函数：通知主线程 HTTP 服务器启动失败
void SignalHttpServerStartFailed() {
    g_httpServerStartedOK.store(false);
    if (g_httpServerStartedEvent) {
        SetEvent(g_httpServerStartedEvent);
    }
}

DWORD WINAPI HttpServerThread(LPVOID lpParam) {
    try {
    int port = g_configManager ? g_configManager->getServerPort() : 35182;
    std::string listenAddr = g_configManager ? g_configManager->getListenAddress() : "0.0.0.0";
    AppendHttpServerLogA("[HttpServerThread] Starting: listen=" + listenAddr + " port=" + std::to_string(port));
    
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AppendHttpServerLogA("[HttpServerThread] ERROR: WSAStartup failed");
        SignalHttpServerStartFailed();
        return 1;
    }
    
    // 创建 socket
    g_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_serverSocket == INVALID_SOCKET) {
        AppendHttpServerLogA("[HttpServerThread] ERROR: socket() failed err=" + std::to_string(WSAGetLastError()));
        WSACleanup();
        SignalHttpServerStartFailed();
        return 1;
    }
    
    // Prefer exclusive binding on Windows to avoid multiple processes "sharing" the same port.
    // Using SO_REUSEADDR for a listening socket is unsafe on Windows.
    int opt = 1;
    if (setsockopt(g_serverSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&opt, sizeof(opt)) != 0) {
        AppendHttpServerLogA("[HttpServerThread] WARN: setsockopt(SO_EXCLUSIVEADDRUSE) failed err=" +
                             std::to_string(WSAGetLastError()));
    }
    
    // 绑定地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    
    if (listenAddr.empty() || listenAddr == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        const int ok = inet_pton(AF_INET, listenAddr.c_str(), &serverAddr.sin_addr);
        if (ok != 1) {
            AppendHttpServerLogA("[HttpServerThread] WARN: invalid listen_address='" + listenAddr +
                                 "', fallback to 0.0.0.0");
            serverAddr.sin_addr.s_addr = INADDR_ANY;
        }
    }
    
    if (bind(g_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int bindErr = WSAGetLastError();
        AppendHttpServerLogA("[HttpServerThread] ERROR: bind() failed listen=" + listenAddr +
                             " port=" + std::to_string(port) +
                             " err=" + std::to_string(bindErr));
        
        // auto_port: 如果配置端口绑定失败且启用了自动端口，尝试随机端口
        bool autoPort = g_configManager ? g_configManager->isAutoPortEnabled() : false;
        if (autoPort && port != 0) {
            AppendHttpServerLogA("[HttpServerThread] auto_port enabled, retrying with port 0...");
            serverAddr.sin_port = htons(0);
            if (bind(g_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                AppendHttpServerLogA("[HttpServerThread] ERROR: bind(port=0) also failed err=" +
                                     std::to_string(WSAGetLastError()));
                closesocket(g_serverSocket);
                WSACleanup();
                SignalHttpServerStartFailed();
                return 1;
            }
            AppendHttpServerLogA("[HttpServerThread] bind(port=0) succeeded");
        } else {
            closesocket(g_serverSocket);
            WSACleanup();
            SignalHttpServerStartFailed();
            return 1;
        }
    }
    
    // 用 getsockname 获取实际绑定的端口（port=0 时由系统分配）
    {
        sockaddr_in boundAddr{};
        int boundLen = sizeof(boundAddr);
        if (getsockname(g_serverSocket, (sockaddr*)&boundAddr, &boundLen) == 0) {
            int actualPort = ntohs(boundAddr.sin_port);
            if (actualPort != port) {
                AppendHttpServerLogA("[HttpServerThread] actual port=" + std::to_string(actualPort) +
                                     " (configured=" + std::to_string(port) + ")");
                port = actualPort;
                // 回写真实端口到配置，以便托盘/状态显示正确
                if (g_configManager) {
                    g_configManager->setActualPort(actualPort);
                }
            }
        }
    }
    
    // 监听
    if (listen(g_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        AppendHttpServerLogA("[HttpServerThread] ERROR: listen() failed err=" + std::to_string(WSAGetLastError()));
        closesocket(g_serverSocket);
        WSACleanup();
        SignalHttpServerStartFailed();
        return 1;
    }

    AppendHttpServerLogA("[HttpServerThread] Listening OK on port " + std::to_string(port));
    
    // 通知主线程启动成功
    g_httpServerStartedOK.store(true);
    if (g_httpServerStartedEvent) {
        SetEvent(g_httpServerStartedEvent);
    }
    
    // 每 IP 每分钟最多 120 次请求（/health 等轻量请求也计入）
    static RateLimiter rateLimiter(120, 60000);

    // 接受连接
    while (g_running) {
        // 设置超时以便能够检查 g_running
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_serverSocket, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(0, &readfds, NULL, NULL, &timeout);
        if (result <= 0) {
            continue;
        }
        
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(g_serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        // 所有连接设发送超时（10 秒），防止 SendAll 在对端不读时卡住 server loop
        int sndTimeout = 10000;
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sndTimeout, sizeof(sndTimeout));

        // Rate limiting: 提取客户端 IP 并检查
        char clientIpBuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIpBuf, sizeof(clientIpBuf));
        std::string clientIp(clientIpBuf);
        if (!rateLimiter.allow(clientIp)) {
            std::string resp = "HTTP/1.1 429 Too Many Requests\r\n"
                               "Content-Type: application/json\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Retry-After: 60\r\n"
                               "Content-Length: 36\r\n"
                               "\r\n"
                               "{\"error\":\"Too many requests\"}";
            SendAll(clientSocket, resp);
            closesocket(clientSocket);
            continue;
        }
        
        // 完整接收 HTTP 请求（header + body）
        std::string request = RecvFullHttpRequest(clientSocket);
        if (request.empty()) {
            closesocket(clientSocket);
            continue;
        }

        // ── SSE 长连接：GET /sse 由专用线程管理 socket 生命周期 ──
        if (IsSseRequest(request)) {
            SseThreadParams* sseParams = new SseThreadParams();
            sseParams->socket = clientSocket;
            sseParams->request = std::move(request);
            HANDLE hThread = CreateThread(NULL, 0, SseConnectionThread, sseParams, 0, NULL);
            if (hThread) {
                CloseHandle(hThread); // 不等待，detach
            } else {
                AppendHttpServerLogA("[HttpServerThread] Failed to create SSE thread");
                delete sseParams;
                closesocket(clientSocket);
            }
            continue; // socket 所有权已转移，不要 close
        }

        // ── 普通请求：请求-响应-关闭 ──
        {
            std::string response;
            try {
                response = HandleHttpRequest(request);
            } catch (const std::exception& e) {
                std::string safe = RedactAuthorizationHeader(request);
                AppendExceptionLogA(std::string("[HttpServerThread] std::exception: ") + e.what());
                AppendExceptionLogA(std::string("[HttpServerThread] request(first 1024): ") + safe.substr(0, 1024));
                const char* errBody = "{\"error\":\"internal_error\"}";
                response = std::string("HTTP/1.1 500 Internal Server Error\r\n")
                    + "Content-Type: application/json\r\n"
                    + "Access-Control-Allow-Origin: *\r\n"
                    + "Content-Length: " + std::to_string(strlen(errBody)) + "\r\n"
                    + "\r\n"
                    + errBody;
            } catch (...) {
                std::string safe = RedactAuthorizationHeader(request);
                AppendExceptionLogA("[HttpServerThread] unknown exception");
                AppendExceptionLogA(std::string("[HttpServerThread] request(first 1024): ") + safe.substr(0, 1024));
                const char* errBody = "{\"error\":\"internal_error\"}";
                response = std::string("HTTP/1.1 500 Internal Server Error\r\n")
                    + "Content-Type: application/json\r\n"
                    + "Access-Control-Allow-Origin: *\r\n"
                    + "Content-Length: " + std::to_string(strlen(errBody)) + "\r\n"
                    + "\r\n"
                    + errBody;
            }
            
            // 发送响应（处理 partial send）
            if (!SendAll(clientSocket, response)) {
                AppendHttpServerLogA("[HttpServerThread] SendAll failed for HTTP response");
            }
        }
        
        closesocket(clientSocket);
    }
    
    closesocket(g_serverSocket);
    WSACleanup();
    AppendHttpServerLogA("[HttpServerThread] Exiting normally");
    return 0;
    
    } catch (const std::exception& e) {
        AppendHttpServerLogA(std::string("[HttpServerThread] FATAL: exception: ") + e.what());
        return 1;
    } catch (...) {
        AppendHttpServerLogA("[HttpServerThread] FATAL: unknown exception");
        return 1;
    }
}

// 添加 Windows 防火墙规则
bool AddFirewallRule() {
    // 获取当前可执行文件路径
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    
    // 构建 netsh 命令
    char command[1024];
    snprintf(command, sizeof(command),
        "netsh advfirewall firewall add rule "
        "name=\"ClawDesk MCP Server\" "
        "dir=in "
        "action=allow "
        "program=\"%s\" "
        "enable=yes "
        "profile=any",
        exePath);
    
    // 执行命令（需要管理员权限）
    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = "runas";  // 请求管理员权限
    sei.lpFile = "cmd.exe";
    
    char params[1200];
    snprintf(params, sizeof(params), "/c %s", command);
    sei.lpParameters = params;
    sei.nShow = SW_HIDE;
    
    if (!ShellExecuteEx(&sei)) {
        return false;
    }
    
    // 等待命令完成
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 5000);
        CloseHandle(sei.hProcess);
    }
    
    return true;
}

// 检查防火墙规则是否存在
bool CheckFirewallRule() {
    // 使用 netsh 检查规则是否存在
    FILE* pipe = _popen("netsh advfirewall firewall show rule name=\"ClawDesk MCP Server\" 2>nul", "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[256];
    bool ruleExists = false;
    
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        if (strstr(buffer, "ClawDesk MCP Server") != NULL) {
            ruleExists = true;
            break;
        }
    }
    
    _pclose(pipe);
    return ruleExists;
}

// 获取本机 IP 地址
std::string GetLocalIPAddress() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "Unknown";
    }
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        WSACleanup();
        return "Unknown";
    }
    
    struct addrinfo hints, *result = nullptr;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        WSACleanup();
        return "Unknown";
    }
    
    std::string ipAddress = "Unknown";
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
        
        // 跳过 127.0.0.1
        if (strcmp(ipStr, "127.0.0.1") != 0) {
            ipAddress = ipStr;
            break;
        }
    }
    
    freeaddrinfo(result);
    WSACleanup();
    
    return ipAddress;
}
