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
#include "http_routes.h"
#include "app_globals.h"
#include "mcp_handlers.h"
#include "mcp_streamable.h"
#include "mcp_sse.h"
#include "support/dashboard_window.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <psapi.h>
#include <gdiplus.h>
#include <nlohmann/json.hpp>
#include "support/config_manager.h"
#include "support/audit_logger.h"
#include "support/license_manager.h"
#include "policy/policy_guard.h"
#include "mcp/tool_registry.h"
#include "services/file_service.h"
#include "services/clipboard_service.h"
#include "services/window_service.h"
#include "services/screenshot_service.h"

using namespace Gdiplus;

struct ParsedRequestLine {
    std::string method;
    std::string path;       // 不含 query string，如 "/list"
    std::string query;      // query string 部分，如 "path=xxx&query=yyy"
    std::string fullLine;   // 原始请求行
};

ParsedRequestLine ParseRequestLine(const std::string& requestLine) {
    ParsedRequestLine parsed;
    parsed.fullLine = requestLine;

    size_t sp1 = requestLine.find(' ');
    if (sp1 == std::string::npos) return parsed;
    parsed.method = requestLine.substr(0, sp1);

    size_t pathStart = sp1 + 1;
    size_t sp2 = requestLine.find(' ', pathStart);
    std::string uri = (sp2 != std::string::npos)
        ? requestLine.substr(pathStart, sp2 - pathStart)
        : requestLine.substr(pathStart);

    size_t qmark = uri.find('?');
    if (qmark != std::string::npos) {
        parsed.path  = uri.substr(0, qmark);
        parsed.query = uri.substr(qmark + 1);
    } else {
        parsed.path = uri;
    }

    return parsed;
}

// 从 query string 中提取指定参数值
std::string GetQueryParam(const std::string& query, const std::string& key) {
    std::string prefix = key + "=";
    size_t pos = 0;
    while (pos < query.size()) {
        size_t found = query.find(prefix, pos);
        if (found == std::string::npos) break;
        if (found > 0 && query[found - 1] != '&') {
            pos = found + 1;
            continue;
        }
        size_t valStart = found + prefix.size();
        size_t valEnd = query.find('&', valStart);
        if (valEnd == std::string::npos) valEnd = query.size();
        return query.substr(valStart, valEnd - valStart);
    }
    return std::string();
}

// 处理 HTTP 请求
std::string HandleHttpRequest(const std::string& request) {
    // 解析请求行
    size_t firstLine = request.find("\r\n");
    if (firstLine == std::string::npos) {
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
    
    std::string requestLine = request.substr(0, firstLine);
    ParsedRequestLine parsed = ParseRequestLine(requestLine);

    // 统一请求日志（Dashboard + HTTP 服务器日志）
    AppendHttpServerLogA("[HTTP] " + parsed.method + " " + parsed.path);
    if (g_dashboard) g_dashboard->logRequest("HTTP", parsed.method + " " + parsed.path);
    
    // 处理 CORS 预检请求
    if (parsed.method == "OPTIONS") {
        return "HTTP/1.1 200 OK\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Session-Id, MCP-Protocol-Version\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }

    // /health 不需要授权，放在授权检查之前
    if (parsed.path == "/health") {
        nlohmann::json health;
        health["status"] = "ok";
        health["version"] = CLAWDESK_VERSION;
        health["uptime_seconds"] = (GetTickCount() - g_startTickCount) / 1000;
        health["sse_sessions"] = SseSessionStore::getInstance().sessionCount();

        // 进程内存信息
        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            health["memory_mb"] = pmc.WorkingSetSize / (1024 * 1024);
        }

        std::string body = health.dump();
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n"
               "\r\n" + body;
    }

    // Auth Token 验证（除 OPTIONS 和 /health 外所有请求都需要）
    if (!IsAuthorizedRequest(request)) {
        return MakeUnauthorizedResponse();
    }

    // ── MCP Streamable HTTP endpoint ──
    if (parsed.path == "/mcp") {
        return HandleMcpStreamableHttp(request);
    }

    // ── MCP SSE transport: POST /messages?sessionId=xxx ──
    if (parsed.path == "/messages" && parsed.method == "POST") {
        return HandleSseMessage(request);
    }
    
    // /reload — 热重载配置文件（不重启进程）
    if (parsed.path == "/reload") {
        if (g_configManager) {
            try {
                g_configManager->load();
                AppendHttpServerLogA("[Reload] Config reloaded successfully");
                if (g_dashboard) g_dashboard->logSuccess("Config", "Reloaded config.json");
                std::string body = "{\"status\":\"reloaded\"}";
                return "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n"
                       "\r\n" + body;
            } catch (const std::exception& e) {
                AppendHttpServerLogA(std::string("[Reload] Failed: ") + e.what());
                if (g_dashboard) g_dashboard->logError("Config", std::string("Reload failed: ") + e.what());
                std::string body = "{\"error\":\"" + std::string(e.what()) + "\"}";
                return "HTTP/1.1 500 Internal Server Error\r\n"
                       "Content-Type: application/json\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n"
                       "\r\n" + body;
            }
        }
        return "HTTP/1.1 500 Internal Server Error\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 32\r\n"
               "\r\n"
               "{\"error\":\"no config manager\"}";
    }

    // 检查是否是退出命令
    if (parsed.path == "/exit") {
        // 设置全局标志
        g_running = false;
        
        // 先在 UI 线程请求关闭 Dashboard 窗口
        CloseDashboardWindow();
        
        // 使用 PostMessage 异步关闭主窗口
        if (g_hwnd) {
            PostMessage(g_hwnd, WM_EXIT_COMMAND, 0, 0);
        }
        if (g_mainThreadId) {
            PostThreadMessage(g_mainThreadId, WM_QUIT, 0, 0);
        }
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 27\r\n"
               "\r\n"
               "{\"status\":\"shutting down\"}";
    }
    
    // 状态检查 - sts 或 status
    if (parsed.path == "/sts" || parsed.path == "/status") {
        
        // 获取电脑名称
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computerName);
        if (!GetComputerNameA(computerName, &size)) {
            strncpy(computerName, "Unknown", sizeof(computerName) - 1);
            computerName[sizeof(computerName) - 1] = '\0';
        }

        int port = g_configManager ? g_configManager->getServerPort() : 35182;
        std::string listenAddr = g_configManager ? g_configManager->getListenAddress() : "0.0.0.0";
        std::string localIP = GetLocalIPAddress();
        std::string licenseType = "opensource";
        
        // 获取运行时间（简化版，实际应该记录启动时间）
        DWORD uptime = GetTickCount() / 1000; // 秒
        
        nlohmann::json out;
        out["status"] = "running";
        out["version"] = CLAWDESK_VERSION;
        out["computer_name"] = std::string(computerName);
        out["port"] = port;
        out["listen_address"] = listenAddr;
        out["local_ip"] = localIP;
        out["license"] = licenseType;
        out["uptime_seconds"] = uptime;
        out["endpoints"] = nlohmann::json::array({
            "/sts", "/status", "/health", "/disks", "/list", "/search", "/read",
            "/clipboard", "/clipboard/image", "/clipboard/file",
            "/screenshot", "/screenshot/file", "/exit"
        });
        std::string body = out.dump();
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "\r\n" + body;
    }
    
    // 获取磁盘列表
    if (parsed.path == "/disks") {

        // Use JSON builder to avoid fixed-size buffers (volume labels can be long).
        nlohmann::json disks = nlohmann::json::array();
        DWORD drives = GetLogicalDrives();

        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i)) {
                char driveLetter[4] = {(char)('A' + i), ':', '\\', '\0'};
                UINT driveType = GetDriveTypeA(driveLetter);

                std::string typeStr = "unknown";
                switch (driveType) {
                    case DRIVE_FIXED: typeStr = "fixed"; break;
                    case DRIVE_REMOVABLE: typeStr = "removable"; break;
                    case DRIVE_REMOTE: typeStr = "network"; break;
                    case DRIVE_CDROM: typeStr = "cdrom"; break;
                    case DRIVE_RAMDISK: typeStr = "ramdisk"; break;
                }

                char volumeName[MAX_PATH] = "";
                char fileSystem[MAX_PATH] = "";
                DWORD serialNumber = 0;
                ULARGE_INTEGER freeBytesAvailable{}, totalBytes{}, totalFreeBytes{};

                GetVolumeInformationA(driveLetter, volumeName, MAX_PATH, &serialNumber,
                                      NULL, NULL, fileSystem, MAX_PATH);
                GetDiskFreeSpaceExA(driveLetter, &freeBytesAvailable, &totalBytes, &totalFreeBytes);

                disks.push_back({
                    {"drive", std::string(1, static_cast<char>('A' + i)) + ":"},
                    {"type", typeStr},
                    {"label", std::string(volumeName)},
                    {"filesystem", std::string(fileSystem)},
                    {"total_bytes", totalBytes.QuadPart},
                    {"free_bytes", totalFreeBytes.QuadPart},
                    {"used_bytes", (totalBytes.QuadPart >= totalFreeBytes.QuadPart)
                        ? (totalBytes.QuadPart - totalFreeBytes.QuadPart)
                        : 0}
                });
            }
        }

        std::string jsonDisks = disks.dump();
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(jsonDisks.length()) + "\r\n"
               "\r\n" + jsonDisks;
    }

    // 读取剪贴板图片文件
    if (parsed.method == "GET" && parsed.path.rfind("/clipboard/image/", 0) == 0) {
        std::string requestPath = parsed.path;

        const std::string prefix = "/clipboard/image/";
        if (requestPath.rfind(prefix, 0) != 0) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        std::string fileName = requestPath.substr(prefix.length());
        if (fileName.empty()) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        for (char c : fileName) {
            if (!(isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) {
                return "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }

        std::string filePath = "clipboard_images/" + fileName;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 21\r\n"
                   "\r\n"
                   "{\"error\":\"not found\"}";
        }

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string data;
        data.resize(static_cast<size_t>(size));
        file.read(&data[0], size);

        char header[256];
        snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: image/png\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "\r\n",
            static_cast<int>(data.size()));

        return std::string(header) + data;
    }

    // 读取截图文件
    if (parsed.method == "GET" && parsed.path.rfind("/screenshot/file/", 0) == 0) {
        std::string requestPath = parsed.path;

        const std::string prefix = "/screenshot/file/";
        if (requestPath.rfind(prefix, 0) != 0) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        std::string fileName = requestPath.substr(prefix.length());
        if (fileName.empty()) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        for (char c : fileName) {
            if (!(isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) {
                return "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }

        std::string filePath = "screenshots/" + fileName;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 21\r\n"
                   "\r\n"
                   "{\"error\":\"not found\"}";
        }

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string data;
        data.resize(static_cast<size_t>(size));
        file.read(&data[0], size);

        const char* contentType = "image/png";
        size_t dot = fileName.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = fileName.substr(dot + 1);
            if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
        }

        char header[256];
        snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "\r\n",
            contentType,
            static_cast<int>(data.size()));

        return std::string(header) + data;
    }

    // 下载剪贴板文件
    if (parsed.method == "GET" && parsed.path.rfind("/clipboard/file/", 0) == 0) {
        std::string requestPath = parsed.path;

        const std::string prefix = "/clipboard/file/";
        if (requestPath.rfind(prefix, 0) != 0) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        std::string fileName = requestPath.substr(prefix.length());
        if (fileName.empty()) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }

        for (char c : fileName) {
            if (!(isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.')) {
                return "HTTP/1.1 400 Bad Request\r\n\r\n";
            }
        }

        std::string filePath = "clipboard_files/" + fileName;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 21\r\n"
                   "\r\n"
                   "{\"error\":\"not found\"}";
        }

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string data;
        data.resize(static_cast<size_t>(size));
        file.read(&data[0], size);

        char header[256];
        snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "\r\n",
            static_cast<int>(data.size()));

        return std::string(header) + data;
    }
    
    // 列出目录内容
    if (parsed.path == "/list") {
        
        // 提取路径参数
        std::string encodedPath = GetQueryParam(parsed.query, "path");
        std::string path = UrlDecode(encodedPath);
        
        if (path.empty()) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 40\r\n"
                   "\r\n"
                   "{\"error\":\"Missing required parameter: path\"}";
        }
        
        // PolicyGuard: 检查目录是否在白名单
        if (g_policyGuard && !g_policyGuard->isPathAllowed(path)) {
            return "HTTP/1.1 403 Forbidden\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 42\r\n"
                   "\r\n"
                   "{\"error\":\"Access denied: path not allowed\"}";
        }
        
        // 确保路径以 \* 结尾用于搜索
        std::string searchPath = path;
        if (!searchPath.empty() && searchPath.back() != '\\') searchPath += "\\";
        searchPath += "*";
        
        nlohmann::json files = nlohmann::json::array();
        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                // 跳过 . 和 ..
                if (strcmp(findData.cFileName, ".") == 0 || 
                    strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }
                
                bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                ULARGE_INTEGER fileSize;
                fileSize.LowPart = findData.nFileSizeLow;
                fileSize.HighPart = findData.nFileSizeHigh;
                
                // 转换文件时间
                FILETIME ft = findData.ftLastWriteTime;
                SYSTEMTIME st;
                FileTimeToSystemTime(&ft, &st);

                char modified[32];
                snprintf(modified, sizeof(modified), "%04d-%02d-%02d %02d:%02d:%02d",
                         st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

                files.push_back({
                    {"name", std::string(findData.cFileName)},
                    {"type", isDir ? "directory" : "file"},
                    {"size", fileSize.QuadPart},
                    {"modified", std::string(modified)}
                });
                
            } while (FindNextFile(hFind, &findData));
            FindClose(hFind);
        }

        std::string jsonFiles = files.dump();

        if (g_dashboard) g_dashboard->logSuccess("list", path + " -> " + std::to_string(files.size()) + " items");
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(jsonFiles.length()) + "\r\n"
               "\r\n" + jsonFiles;
    }
    
    // 在文件中搜索
    if (parsed.path == "/search") {
        
        // 提取路径参数
        std::string encodedPath = GetQueryParam(parsed.query, "path");
        std::string filepath = UrlDecode(encodedPath);
        
        // 提取搜索关键词
        std::string query = UrlDecode(GetQueryParam(parsed.query, "query"));
        
        if (query.empty()) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 42\r\n"
                   "\r\n"
                   "{\"error\":\"Missing required parameter: query\"}";
        }
        
        // PolicyGuard: 检查文件路径是否在白名单
        if (g_policyGuard && !g_policyGuard->isPathAllowed(filepath)) {
            return "HTTP/1.1 403 Forbidden\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 42\r\n"
                   "\r\n"
                   "{\"error\":\"Access denied: path not allowed\"}";
        }
        
        // 提取可选参数
        bool caseInsensitive = false;
        int maxResults = 100;
        int contextLines = 0;
        
        std::string caseStr = GetQueryParam(parsed.query, "case");
        if (!caseStr.empty()) {
            caseInsensitive = (caseStr == "insensitive" || caseStr == "i");
        }
        
        std::string maxStr = GetQueryParam(parsed.query, "max");
        if (!maxStr.empty()) {
            maxResults = atoi(maxStr.c_str());
            if (maxResults <= 0) maxResults = 100;
            if (maxResults > 1000) maxResults = 1000;
        }
        
        std::string contextStr = GetQueryParam(parsed.query, "context");
        if (!contextStr.empty()) {
            contextLines = atoi(contextStr.c_str());
            if (contextLines < 0) contextLines = 0;
            if (contextLines > 10) contextLines = 10;
        }
        
        // 打开文件
        FILE* file = fopen(filepath.c_str(), "rb");
        if (!file) {
            return "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 38\r\n"
                   "\r\n"
                   "{\"error\":\"File not found or access denied\"}";
        }
        
        // 获取文件大小并检查限制
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        const long MAX_FILE_SIZE = 10 * 1024 * 1024;
        if (fileSize > MAX_FILE_SIZE) {
            fclose(file);
                return "HTTP/1.1 413 Payload Too Large\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 52\r\n"
                   "\r\n"
                   "{\"error\":\"File too large\",\"max_size\":\"10MB\"}";
        }
        
        // 读取并分割行
        std::vector<std::string> lines;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), file)) {
            lines.push_back(buffer);
        }
        fclose(file);
        
        // 准备搜索
        std::string searchQuery = query;
        if (caseInsensitive) {
            for (char& c : searchQuery) c = tolower(c);
        }
        
        // 搜索匹配
        nlohmann::json matchesArr = nlohmann::json::array();
        int matchCount = 0;
        
        for (int i = 0; i < (int)lines.size() && matchCount < maxResults; i++) {
            std::string searchLine = lines[i];
            if (caseInsensitive) {
                for (char& c : searchLine) c = tolower(c);
            }
            
            if (searchLine.find(searchQuery) != std::string::npos) {
                matchesArr.push_back({
                    {"line_number", i},
                    {"content", lines[i]}
                });
                matchCount++;
            }
        }
        
        // 构建响应
        nlohmann::json respJson;
        respJson["path"] = filepath;
        respJson["query"] = query;
        respJson["total_lines"] = (int)lines.size();
        respJson["match_count"] = matchCount;
        respJson["case_sensitive"] = !caseInsensitive;
        respJson["matches"] = matchesArr;
        std::string jsonResponse = respJson.dump();
        
        if (g_dashboard) g_dashboard->logSuccess("search", filepath + " -> " + std::to_string(matchCount) + " matches");
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
            "\r\n" + jsonResponse;
        
        return response;
    }
    
    // 读取文件内容
    if (parsed.path == "/read") {
        
        // 提取路径参数
        std::string encodedPath = GetQueryParam(parsed.query, "path");
        std::string filepath = UrlDecode(encodedPath);
        
        if (filepath.empty()) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 40\r\n"
                   "\r\n"
                   "{\"error\":\"Missing required parameter: path\"}";
        }
        
        // PolicyGuard: 检查文件路径是否在白名单
        if (g_policyGuard && !g_policyGuard->isPathAllowed(filepath)) {
            return "HTTP/1.1 403 Forbidden\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 42\r\n"
                   "\r\n"
                   "{\"error\":\"Access denied: path not allowed\"}";
        }
        
        // 提取可选参数
        int startLine = 0;
        int maxLines = -1;
        int tailLines = -1;
        bool countOnly = false;
        
        std::string startStr = GetQueryParam(parsed.query, "start");
        if (!startStr.empty()) startLine = atoi(startStr.c_str());
        
        std::string linesStr = GetQueryParam(parsed.query, "lines");
        if (!linesStr.empty()) maxLines = atoi(linesStr.c_str());
        
        std::string tailStr = GetQueryParam(parsed.query, "tail");
        if (!tailStr.empty()) tailLines = atoi(tailStr.c_str());
        
        std::string countStr = GetQueryParam(parsed.query, "count");
        if (!countStr.empty()) countOnly = (countStr == "true" || countStr == "1");
        
        // 打开文件
        FILE* file = fopen(filepath.c_str(), "rb");
        if (!file) {
            return "HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 38\r\n"
                   "\r\n"
                   "{\"error\":\"File not found or access denied\"}";
        }
        
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // 检查文件大小限制（10MB）
        const long MAX_FILE_SIZE = 10 * 1024 * 1024;
        if (fileSize > MAX_FILE_SIZE) {
            fclose(file);
            return "HTTP/1.1 413 Payload Too Large\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 52\r\n"
                   "\r\n"
                   "{\"error\":\"File too large\",\"max_size\":\"10MB\"}";
        }
        
        // 读取文件内容
        std::string content;
        content.reserve(fileSize);
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), file)) {
            content += buffer;
        }
        fclose(file);
        
        // 按行分割
        std::vector<std::string> lines;
        size_t pos = 0;
        while (pos < content.length()) {
            size_t newlinePos = content.find('\n', pos);
            if (newlinePos == std::string::npos) {
                lines.push_back(content.substr(pos));
                break;
            }
            lines.push_back(content.substr(pos, newlinePos - pos + 1));
            pos = newlinePos + 1;
        }
        
        int totalLines = lines.size();
        
        // 如果只是获取行数
        if (countOnly) {
            nlohmann::json out;
            out["path"] = filepath;
            out["total_lines"] = totalLines;
            out["file_size"] = fileSize;
            std::string jsonResponse = out.dump();
            
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
                   "\r\n" + jsonResponse;
        }
        
        // 应用起始行和行数限制
        int actualStart = 0;
        int actualEnd = totalLines;
        
        if (tailLines > 0) {
            // 从尾部读取
            actualStart = (totalLines > tailLines) ? (totalLines - tailLines) : 0;
            actualEnd = totalLines;
        } else {
            // 从指定位置读取
            actualStart = (startLine < 0) ? 0 : startLine;
            if (actualStart >= totalLines) {
                actualStart = totalLines;
                actualEnd = totalLines;
            } else if (maxLines > 0) {
                actualEnd = actualStart + maxLines;
                if (actualEnd > totalLines) actualEnd = totalLines;
            }
        }
        
        // 构建返回的内容
        std::string resultContent;
        for (int i = actualStart; i < actualEnd; i++) {
            resultContent += lines[i];
        }
        
        nlohmann::json out;
        out["path"] = filepath;
        out["total_lines"] = totalLines;
        out["start_line"] = actualStart;
        out["returned_lines"] = (actualEnd - actualStart);
        out["file_size"] = fileSize;
        out["content"] = resultContent;
        if (g_dashboard) g_dashboard->logSuccess("read", filepath + " (" + std::to_string(actualEnd - actualStart) + " lines)");
        std::string jsonResponse = out.dump();
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
               "\r\n" + jsonResponse;
    }
    
    // 获取剪贴板内容
    if (parsed.path == "/clipboard") {
        if (!OpenClipboard(NULL)) {
            
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 40\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to open clipboard\"}";
        }
        
        std::string imagePath;
        if (SaveClipboardImageFromOpenClipboard(imagePath)) {
            CloseClipboard();

            std::string fileName = imagePath;
            size_t slash = fileName.find_last_of("/\\");
            if (slash != std::string::npos) {
                fileName = fileName.substr(slash + 1);
            }

            std::string urlPath = "/clipboard/image/" + fileName;
            std::string url = BuildUrlFromRequest(request, urlPath);
            nlohmann::json imgJson;
            imgJson["type"] = "image";
            imgJson["format"] = "png";
            imgJson["url"] = url;
            imgJson["path"] = urlPath;
            std::string jsonResponse = imgJson.dump();

            std::string response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
                "\r\n" + jsonResponse;

            return response;
        }

        std::vector<std::string> filePaths;
        std::vector<std::string> fileNames;
        if (SaveClipboardFilesFromOpenClipboard(filePaths, fileNames)) {
            CloseClipboard();

            nlohmann::json filesJson;
            filesJson["type"] = "files";
            filesJson["files"] = nlohmann::json::array();
            for (size_t i = 0; i < fileNames.size(); ++i) {
                std::string urlPath = "/clipboard/file/" + fileNames[i];
                std::string url = BuildUrlFromRequest(request, urlPath);
                filesJson["files"].push_back({{"name", fileNames[i]}, {"url", url}, {"path", urlPath}});
            }
            std::string jsonResponse = filesJson.dump();

            std::string response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
                "\r\n" + jsonResponse;

            return response;
        }

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (!hData) {
            CloseClipboard();
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 32\r\n"
                   "\r\n"
                   "{\"content\":\"\",\"empty\":true}";
        }
        
        char* pData = (char*)GlobalLock(hData);
        if (!pData) {
            CloseClipboard();
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 47\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to lock clipboard data\"}";
        }
        
        std::string clipboardText(pData);
        GlobalUnlock(hData);
        CloseClipboard();
        
        nlohmann::json clipJson;
        clipJson["content"] = clipboardText;
        clipJson["length"] = clipboardText.length();
        clipJson["empty"] = false;
        std::string jsonResponse = clipJson.dump();
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
            "\r\n" + jsonResponse;
        
        return response;
    }
    
    // 设置剪贴板内容（需要 POST 请求体）
    if (parsed.method == "PUT" && parsed.path == "/clipboard") {
        
        // 提取请求体中的内容
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 35\r\n"
                   "\r\n"
                   "{\"error\":\"Missing request body\"}";
        }
        
        std::string body = request.substr(bodyStart + 4);
        
        // 使用 nlohmann::json 解析请求体
        nlohmann::json reqJson;
        try {
            reqJson = nlohmann::json::parse(body);
        } catch (const std::exception&) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 36\r\n"
                   "\r\n"
                   "{\"error\":\"Invalid JSON format\"}";
        }
        
        if (!reqJson.contains("content") || !reqJson["content"].is_string()) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 44\r\n"
                   "\r\n"
                   "{\"error\":\"Missing 'content' field in JSON\"}";
        }
        
        std::string decodedContent = reqJson["content"].get<std::string>();
        
        // 设置剪贴板
        if (!OpenClipboard(NULL)) {
            
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 40\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to open clipboard\"}";
        }
        
        EmptyClipboard();
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, decodedContent.length() + 1);
        if (!hMem) {
            CloseClipboard();
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 42\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to allocate memory\"}";
        }
        
        char* pMem = (char*)GlobalLock(hMem);
        if (!pMem) {
            GlobalFree(hMem);
            CloseClipboard();
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 46\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to lock clipboard memory\"}";
        }
        memcpy(pMem, decodedContent.c_str(), decodedContent.length());
        pMem[decodedContent.length()] = '\0';
        GlobalUnlock(hMem);
        
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();
        
        std::string jsonResponse = "{\"success\":true,\"length\":" + 
            std::to_string(decodedContent.length()) + "}";
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
            "\r\n" + jsonResponse;
        
        return response;
    }
    
    // 获取窗口列表
    if (parsed.path == "/windows") {
        std::string windowList = GetWindowList();
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(windowList.length()) + "\r\n"
            "\r\n" + windowList;
        
        return response;
    }
    
    // 获取进程列表
    if (parsed.path == "/processes") {
        std::string processList = GetProcessList();
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(processList.length()) + "\r\n"
            "\r\n" + processList;
        
        return response;
    }
    
    // MCP 协议：初始化
    if (parsed.method == "POST" && parsed.path == "/mcp/initialize") {
        size_t bodyStart = request.find("\r\n\r\n");
        std::string body = (bodyStart != std::string::npos) ? request.substr(bodyStart + 4) : "{}";
        
        std::string result = HandleMCPInitialize(body);
        if (g_dashboard) g_dashboard->logSuccess("MCP-REST", "initialize");
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(result.length()) + "\r\n"
            "\r\n" + result;
        
        return response;
    }
    
    // MCP 协议：列出工具
    if (parsed.method == "POST" && parsed.path == "/mcp/tools/list") {
        std::string result = HandleMCPToolsList();
        if (g_dashboard) g_dashboard->logSuccess("MCP-REST", "tools/list");
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(result.length()) + "\r\n"
            "\r\n" + result;
        
        return response;
    }
    
    // MCP 协议：调用工具
    if (parsed.method == "POST" && parsed.path == "/mcp/tools/call") {
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 35\r\n"
                   "\r\n"
                   "{\"error\":\"Missing request body\"}";
        }
        
        std::string body = request.substr(bodyStart + 4);
        std::string result = HandleMCPToolsCall(body);
        if (g_dashboard) g_dashboard->logSuccess("MCP-REST", "tools/call");
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(result.length()) + "\r\n"
            "\r\n" + result;
        
        return response;
    }
    
    // 执行命令
    if (parsed.method == "POST" && parsed.path == "/execute") {
        
        // 提取请求体中的命令
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart == std::string::npos) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 35\r\n"
                   "\r\n"
                   "{\"error\":\"Missing request body\"}";
        }
        
        std::string body = request.substr(bodyStart + 4);
        
        // 使用 nlohmann::json 解析请求体
        nlohmann::json reqJson;
        try {
            reqJson = nlohmann::json::parse(body);
        } catch (const std::exception& e) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 36\r\n"
                   "\r\n"
                   "{\"error\":\"Invalid JSON format\"}";
        }
        
        if (!reqJson.contains("command") || !reqJson["command"].is_string()) {
            return "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 45\r\n"
                   "\r\n"
                   "{\"error\":\"Missing 'command' field in JSON\"}";
        }
        
        std::string decodedCommand = reqJson["command"].get<std::string>();
        
        // PolicyGuard: 检查命令是否在白名单
        if (g_policyGuard && !g_policyGuard->isCommandAllowed(decodedCommand)) {
            return "HTTP/1.1 403 Forbidden\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 45\r\n"
                   "\r\n"
                   "{\"error\":\"Access denied: command not allowed\"}";
        }
        
        // 执行命令
        if (g_dashboard) g_dashboard->logProcessing("execute", decodedCommand);
        std::string result = ExecuteCommand(decodedCommand);
        if (g_dashboard) g_dashboard->logSuccess("execute", decodedCommand);
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(result.length()) + "\r\n"
            "\r\n" + result;
        
        return response;
    }
    
    // 截图功能
    if (parsed.path == "/screenshot") {
        // 提取格式参数（可选）
        std::string format = GetQueryParam(parsed.query, "format");
        if (format.empty()) format = "png";
        
        // 捕获截图
        std::string imagePath;
        int screenWidth = 0;
        int screenHeight = 0;
        if (!SaveScreenshotToFile(format, imagePath, screenWidth, screenHeight)) {
            if (g_dashboard) g_dashboard->logError("screenshot", "Failed to capture");
            return "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Type: application/json\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Content-Length: 43\r\n"
                   "\r\n"
                   "{\"error\":\"Failed to capture screenshot\"}";
        }
        
        std::string fileName = imagePath;
        size_t slash = fileName.find_last_of("/\\");
        if (slash != std::string::npos) {
            fileName = fileName.substr(slash + 1);
        }
        
        std::string urlPath = "/screenshot/file/" + fileName;
        std::string url = BuildUrlFromRequest(request, urlPath);
        
        nlohmann::json ssJson;
        ssJson["success"] = true;
        ssJson["format"] = format;
        ssJson["width"] = screenWidth;
        ssJson["height"] = screenHeight;
        ssJson["url"] = url;
        ssJson["path"] = urlPath;
        if (g_dashboard) g_dashboard->logSuccess("screenshot", format + " " + std::to_string(screenWidth) + "x" + std::to_string(screenHeight));
        std::string jsonResponse = ssJson.dump();
        
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(jsonResponse.length()) + "\r\n"
            "\r\n" + jsonResponse;
        
        return response;
    }
    
    // 根路径或 /help - 显示欢迎信息和 API 列表
    if (parsed.path == "/" || parsed.path == "/help") {
        std::string body = BuildHelpJson();
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(body.length()) + "\r\n"
            "\r\n" + body;
        return response;
    }
    
    // 默认响应
    nlohmann::json notFound;
    notFound["error"] = "Not Found";
    notFound["available_endpoints"] = {"/", "/help", "/sts", "/status", "/health",
        "/disks", "/list", "/search", "/read", "/clipboard", "/clipboard/image",
        "/clipboard/file", "/screenshot", "/screenshot/file", "/windows",
        "/processes", "/execute", "/sse", "/messages", "/mcp",
        "/mcp/initialize", "/mcp/tools/list", "/mcp/tools/call", "/exit"};
    std::string notFoundBody = notFound.dump();
    char notFoundHeader[512];
    snprintf(notFoundHeader, sizeof(notFoundHeader),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %zu\r\n"
        "\r\n",
        notFoundBody.size());
    return std::string(notFoundHeader) + notFoundBody;
}
