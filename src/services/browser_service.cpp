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
#include "services/browser_service.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <wincrypt.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cwctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

static std::wstring Utf8ToWideSimple(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out;
    out.resize((size_t)len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

static std::string WideToUtf8Simple(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out;
    out.resize((size_t)len);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, nullptr, nullptr);
    return out;
}

static bool FileExistsW(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static std::wstring NormalizePathSeparatorsW(std::wstring p) {
    for (auto& ch : p) {
        if (ch == L'/') ch = L'\\';
    }
    return p;
}

static std::wstring SanitizePathComponentW(const std::wstring& input) {
    std::wstring out;
    out.reserve(input.size());
    for (wchar_t c : input) {
        if ((c >= L'0' && c <= L'9') ||
            (c >= L'A' && c <= L'Z') ||
            (c >= L'a' && c <= L'z') ||
            c == L'-' || c == L'_') {
            out.push_back(c);
        } else {
            out.push_back(L'_');
        }
    }
    return out;
}

static std::wstring JoinPathW(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

static std::wstring GetTempDirW() {
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = GetTempPathW(MAX_PATH, buf);
    if (n == 0 || n >= MAX_PATH) {
        return L"C:\\Temp\\";
    }
    return std::wstring(buf);
}

static void EnsureDirW(const std::wstring& path) {
    if (path.empty()) return;
    CreateDirectoryW(path.c_str(), NULL);
}

static std::wstring TryResolveChromeExePath(const std::wstring& input) {
    std::wstring candidate = NormalizePathSeparatorsW(input);
    if (FileExistsW(candidate)) {
        return candidate;
    }

    std::wstring base = candidate;
    size_t slash = base.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        base = base.substr(slash + 1);
    }
    std::wstring lower = base;
    for (auto& ch : lower) ch = (wchar_t)towlower(ch);
    if (lower == L"google chrome") lower = L"chrome.exe";
    if (lower == L"chrome") lower = L"chrome.exe";
    if (lower != L"chrome.exe") {
        return L"";
    }

    // Try SearchPath first (PATH/App Paths may resolve).
    wchar_t found[MAX_PATH] = {0};
    DWORD n = SearchPathW(NULL, L"chrome.exe", NULL, MAX_PATH, found, NULL);
    if (n > 0 && n < MAX_PATH && FileExistsW(found)) {
        return found;
    }

    // Try common install locations.
    struct KnownFolder {
        REFKNOWNFOLDERID id;
        const wchar_t* suffix;
    };
    const KnownFolder kFolders[] = {
        {FOLDERID_ProgramFiles, L"\\Google\\Chrome\\Application\\chrome.exe"},
        {FOLDERID_ProgramFilesX86, L"\\Google\\Chrome\\Application\\chrome.exe"},
        {FOLDERID_LocalAppData, L"\\Google\\Chrome\\Application\\chrome.exe"},
    };

    for (const auto& k : kFolders) {
        PWSTR folder = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(k.id, 0, NULL, &folder)) && folder) {
            std::wstring p(folder);
            CoTaskMemFree(folder);
            p += k.suffix;
            if (FileExistsW(p)) {
                return p;
            }
        }
    }

    return L"";
}

static std::string HttpGetLocalhostJson(int port, const std::wstring& path) {
    HINTERNET hSession = WinHttpOpen(L"WinBridgeAgent-Browser/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!hSession) {
        throw std::runtime_error("WinHttpOpen failed");
    }
    auto closeSession = [&]() { WinHttpCloseHandle(hSession); };

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", (INTERNET_PORT)port, 0);
    if (!hConnect) {
        closeSession();
        throw std::runtime_error("WinHttpConnect failed");
    }
    auto closeConnect = [&]() { WinHttpCloseHandle(hConnect); };

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                            L"GET",
                                            path.c_str(),
                                            NULL,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            0);
    if (!hRequest) {
        closeConnect();
        closeSession();
        throw std::runtime_error("WinHttpOpenRequest failed");
    }
    auto closeRequest = [&]() { WinHttpCloseHandle(hRequest); };

    DWORD timeoutMs = 5000;
    WinHttpSetTimeouts(hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    if (!WinHttpSendRequest(hRequest,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            WINHTTP_NO_REQUEST_DATA,
                            0,
                            0,
                            0)) {
        closeRequest();
        closeConnect();
        closeSession();
        throw std::runtime_error("WinHttpSendRequest failed");
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        closeRequest();
        closeConnect();
        closeSession();
        throw std::runtime_error("WinHttpReceiveResponse failed");
    }

    std::string body;
    while (true) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
            break;
        }
        if (avail == 0) {
            break;
        }
        std::string chunk;
        chunk.resize(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, &chunk[0], avail, &read)) {
            break;
        }
        if (read == 0) {
            break;
        }
        chunk.resize(read);
        body += chunk;
    }

    closeRequest();
    closeConnect();
    closeSession();
    return body;
}

static std::string ResolveBrowserWebSocketUrl(int port) {
    std::string body = HttpGetLocalhostJson(port, L"/json/version");
    auto j = nlohmann::json::parse(body);
    std::string ws = j.value("webSocketDebuggerUrl", "");
    if (ws.empty()) {
        throw std::runtime_error("DevTools /json/version missing webSocketDebuggerUrl");
    }
    return ws;
}

struct WsUrlParts {
    std::wstring host;
    INTERNET_PORT port = 0;
    std::wstring path;
    bool secure = false;
};

static WsUrlParts ParseWsUrl(const std::string& wsUrl) {
    // ws://host:port/path or wss://host:port/path
    std::string s = wsUrl;
    WsUrlParts parts;
    if (s.rfind("wss://", 0) == 0) {
        parts.secure = true;
        s = s.substr(6);
    } else if (s.rfind("ws://", 0) == 0) {
        parts.secure = false;
        s = s.substr(5);
    } else {
        throw std::runtime_error("Invalid websocket url");
    }

    size_t slash = s.find('/');
    std::string hostport = (slash == std::string::npos) ? s : s.substr(0, slash);
    std::string path = (slash == std::string::npos) ? "/" : s.substr(slash);

    std::string host = hostport;
    int port = parts.secure ? 443 : 80;
    size_t colon = hostport.find(':');
    if (colon != std::string::npos) {
        host = hostport.substr(0, colon);
        port = atoi(hostport.substr(colon + 1).c_str());
    }

    parts.host = Utf8ToWideSimple(host);
    parts.port = (INTERNET_PORT)port;
    parts.path = Utf8ToWideSimple(path);
    return parts;
}

struct WebSocketConn {
    HINTERNET ws = NULL;
    HINTERNET request = NULL;
    HINTERNET connect = NULL;
    HINTERNET session = NULL;
};

static WebSocketConn OpenWebSocket(const WsUrlParts& urlParts) {
    DWORD flags = urlParts.secure ? WINHTTP_FLAG_SECURE : 0;
    WebSocketConn conn{};

    conn.session = WinHttpOpen(L"WinBridgeAgent-Browser/1.0",
                               WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS,
                               0);
    if (!conn.session) {
        throw std::runtime_error("WinHttpOpen failed");
    }

    conn.connect = WinHttpConnect(conn.session, urlParts.host.c_str(), urlParts.port, 0);
    if (!conn.connect) {
        WinHttpCloseHandle(conn.session);
        throw std::runtime_error("WinHttpConnect failed");
    }

    conn.request = WinHttpOpenRequest(conn.connect,
                                      L"GET",
                                      urlParts.path.c_str(),
                                      NULL,
                                      WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      flags);
    if (!conn.request) {
        WinHttpCloseHandle(conn.connect);
        WinHttpCloseHandle(conn.session);
        throw std::runtime_error("WinHttpOpenRequest failed");
    }

    DWORD timeoutMs = 8000;
    WinHttpSetTimeouts(conn.request, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    if (!WinHttpSetOption(conn.request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
        WinHttpCloseHandle(conn.request);
        WinHttpCloseHandle(conn.connect);
        WinHttpCloseHandle(conn.session);
        throw std::runtime_error("WinHttpSetOption(UPGRADE_TO_WEB_SOCKET) failed");
    }

    if (!WinHttpSendRequest(conn.request,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            WINHTTP_NO_REQUEST_DATA,
                            0,
                            0,
                            0)) {
        WinHttpCloseHandle(conn.request);
        WinHttpCloseHandle(conn.connect);
        WinHttpCloseHandle(conn.session);
        throw std::runtime_error("WinHttpSendRequest failed");
    }
    if (!WinHttpReceiveResponse(conn.request, NULL)) {
        WinHttpCloseHandle(conn.request);
        WinHttpCloseHandle(conn.connect);
        WinHttpCloseHandle(conn.session);
        throw std::runtime_error("WinHttpReceiveResponse failed");
    }

    conn.ws = WinHttpWebSocketCompleteUpgrade(conn.request, 0);
    WinHttpCloseHandle(conn.request);
    conn.request = NULL;

    if (!conn.ws) {
        WinHttpCloseHandle(conn.connect);
        WinHttpCloseHandle(conn.session);
        conn.connect = NULL;
        conn.session = NULL;
        throw std::runtime_error("WinHttpWebSocketCompleteUpgrade failed");
    }
    return conn;
}

static nlohmann::json SendCdpCommand(const std::string& wsUrl,
                                     const std::string& method,
                                     const nlohmann::json& params) {
    WsUrlParts parts = ParseWsUrl(wsUrl);
    WebSocketConn conn = OpenWebSocket(parts);
    auto closeConn = [&]() {
        if (conn.ws) {
            WinHttpWebSocketClose(conn.ws, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
            WinHttpCloseHandle(conn.ws);
            conn.ws = NULL;
        }
        if (conn.connect) {
            WinHttpCloseHandle(conn.connect);
            conn.connect = NULL;
        }
        if (conn.session) {
            WinHttpCloseHandle(conn.session);
            conn.session = NULL;
        }
    };

    nlohmann::json req;
    req["id"] = 1;
    req["method"] = method;
    if (!params.is_null()) {
        req["params"] = params;
    }
    std::string payload = req.dump();

    DWORD res = WinHttpWebSocketSend(conn.ws,
                                    WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                    (void*)payload.data(),
                                    (DWORD)payload.size());
    if (res != NO_ERROR) {
        closeConn();
        throw std::runtime_error("WinHttpWebSocketSend failed");
    }

    std::string message;
    message.reserve(16 * 1024);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        BYTE buffer[64 * 1024];
        DWORD bytesRead = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
        res = WinHttpWebSocketReceive(conn.ws, buffer, sizeof(buffer), &bytesRead, &type);
        if (res != NO_ERROR) {
            closeConn();
            throw std::runtime_error("WinHttpWebSocketReceive failed");
        }
        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            break;
        }
        if (bytesRead > 0) {
            message.append((const char*)buffer, (size_t)bytesRead);
        }
        // Messages can be fragmented; once we have something that parses and matches id, return it.
        try {
            auto j = nlohmann::json::parse(message);
            if (j.contains("id") && j["id"].is_number_integer() && j["id"].get<int>() == 1) {
                closeConn();
                return j;
            }
            // Ignore event messages and keep receiving.
            message.clear();
        } catch (...) {
            // Need more data.
        }
    }

    closeConn();
    throw std::runtime_error("Timeout waiting for CDP response");
}

static std::string ResolveTargetWebSocketUrl(int port, const std::string& targetId) {
    std::string listBody = HttpGetLocalhostJson(port, L"/json/list");
    auto arr = nlohmann::json::parse(listBody);
    if (!arr.is_array()) {
        throw std::runtime_error("/json/list did not return array");
    }
    for (auto& t : arr) {
        if (!t.is_object()) continue;
        if (t.value("id", "") == targetId) {
            std::string ws = t.value("webSocketDebuggerUrl", "");
            if (!ws.empty()) return ws;
        }
    }
    throw std::runtime_error("Target not found");
}

static nlohmann::json FindTargetInfoById(int port, const std::string& targetId) {
    std::string listBody = HttpGetLocalhostJson(port, L"/json/list");
    auto arr = nlohmann::json::parse(listBody);
    if (!arr.is_array()) {
        throw std::runtime_error("/json/list did not return array");
    }
    for (auto& t : arr) {
        if (!t.is_object()) continue;
        if (t.value("id", "") == targetId) {
            return t;
        }
    }
    throw std::runtime_error("Target not found");
}

static std::string BuildDevToolsUrlFromTarget(int port, const nlohmann::json& target) {
    std::string frontend = target.value("devtoolsFrontendUrl", "");
    if (!frontend.empty()) {
        if (frontend.rfind("http://", 0) == 0 || frontend.rfind("https://", 0) == 0) {
            return frontend;
        }
        if (frontend.front() != '/') {
            frontend = "/" + frontend;
        }
        return std::string("http://127.0.0.1:") + std::to_string(port) + frontend;
    }

    // Fallback build from websocket url.
    std::string ws = target.value("webSocketDebuggerUrl", "");
    if (ws.empty()) {
        return "";
    }
    // ws://127.0.0.1:9222/devtools/page/<id>
    // inspector URL is usually:
    // http://127.0.0.1:9222/devtools/inspector.html?ws=127.0.0.1:9222/devtools/page/<id>
    std::string hostPart = "127.0.0.1:" + std::to_string(port);
    std::string wsPart = hostPart;
    size_t scheme = ws.find("://");
    std::string rest = (scheme == std::string::npos) ? ws : ws.substr(scheme + 3);
    size_t slash = rest.find('/');
    std::string path = (slash == std::string::npos) ? "" : rest.substr(slash);
    if (path.empty()) {
        return "";
    }
    return std::string("http://127.0.0.1:") + std::to_string(port)
        + "/devtools/inspector.html?ws=" + wsPart + path;
}

static std::string EnsureScreenshotsDirAndName() {
    CreateDirectoryA("screenshots", NULL);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[160];
    snprintf(filename, sizeof(filename), "screenshots/browser_%04d%02d%02d_%02d%02d%02d.png",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return filename;
}

static std::vector<uint8_t> Base64DecodeToBytes(const std::string& b64) {
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(b64.c_str(),
                              (DWORD)b64.size(),
                              CRYPT_STRING_BASE64,
                              NULL,
                              &outLen,
                              NULL,
                              NULL)) {
        throw std::runtime_error("CryptStringToBinaryA(size) failed");
    }
    std::vector<uint8_t> out(outLen);
    if (!CryptStringToBinaryA(b64.c_str(),
                              (DWORD)b64.size(),
                              CRYPT_STRING_BASE64,
                              out.data(),
                              &outLen,
                              NULL,
                              NULL)) {
        throw std::runtime_error("CryptStringToBinaryA(data) failed");
    }
    out.resize(outLen);
    return out;
}

BrowserService::~BrowserService() {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& kv : sessions_) {
        HANDLE h = (HANDLE)kv.second.processHandle;
        if (h) {
            TerminateProcess(h, 0);
            CloseHandle(h);
        }
    }
    sessions_.clear();
}

std::string BrowserService::generateSessionId() {
    GUID g{};
    if (CoCreateGuid(&g) != S_OK) {
        // Fallback.
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buf[64];
        snprintf(buf, sizeof(buf), "sess_%04d%02d%02d%02d%02d%02d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return buf;
    }
    wchar_t wbuf[64];
    StringFromGUID2(g, wbuf, 64);
    return WideToUtf8Simple(wbuf);
}

int BrowserService::pickFreePort() {
    WSADATA wsa{};
    bool wsaOk = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    // Bind to port 0, read the assigned port, then close.
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (wsaOk) WSACleanup();
        return 9222;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        if (wsaOk) WSACleanup();
        return 9222;
    }
    int len = sizeof(addr);
    if (getsockname(s, (sockaddr*)&addr, &len) != 0) {
        closesocket(s);
        if (wsaOk) WSACleanup();
        return 9222;
    }
    int port = ntohs(addr.sin_port);
    closesocket(s);
    if (wsaOk) WSACleanup();
    if (port <= 0) port = 9222;
    return port;
}

BrowserService::LaunchResult BrowserService::launch(const std::string& app,
                                                    bool headless,
                                                    const std::string& user_data_dir,
                                                    const std::vector<std::string>& additional_args) {
    LaunchResult out;
    out.session_id = generateSessionId();
    out.port = pickFreePort();

    std::wstring wApp = Utf8ToWideSimple(app);
    if (wApp.empty()) {
        out.success = false;
        out.error = "Invalid app";
        return out;
    }
    wApp = NormalizePathSeparatorsW(wApp);

    // Best-effort resolution for Chrome to make deployment stable.
    if (!FileExistsW(wApp)) {
        std::wstring resolvedChrome = TryResolveChromeExePath(wApp);
        if (!resolvedChrome.empty()) {
            wApp = resolvedChrome;
        }
    }

    if (!FileExistsW(wApp)) {
        out.success = false;
        out.error = "Browser executable not found";
        return out;
    }

    std::wstring wUserData = Utf8ToWideSimple(user_data_dir);
    if (wUserData.empty()) {
        // Always use a distinct profile directory to ensure Chrome starts a new instance with
        // remote-debugging enabled, even if a user Chrome instance is already running.
        std::wstring base = JoinPathW(GetTempDirW(), L"WinBridgeAgentProfiles");
        EnsureDirW(base);
        std::wstring comp = SanitizePathComponentW(Utf8ToWideSimple(out.session_id));
        if (comp.empty()) comp = L"profile";
        wUserData = JoinPathW(base, comp);
        EnsureDirW(wUserData);
    }

    std::wstring args = L"--remote-debugging-address=127.0.0.1 ";
    args += L"--remote-debugging-port=" + std::to_wstring(out.port) + L" ";
    args += L"--no-first-run --no-default-browser-check ";
    if (headless) {
        args += L"--headless=new ";
    }
    args += L"--user-data-dir=\"" + wUserData + L"\" ";
    for (const auto& a : additional_args) {
        std::wstring wa = Utf8ToWideSimple(a);
        if (!wa.empty()) {
            args += wa + L" ";
        }
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = wApp.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = headless ? SW_HIDE : SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei) || !sei.hProcess) {
        out.success = false;
        out.error = "Failed to launch browser";
        return out;
    }

    out.pid = GetProcessId(sei.hProcess);

    // Poll until DevTools HTTP endpoint is ready.
    bool ready = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        try {
            (void)HttpGetLocalhostJson(out.port, L"/json/version");
            ready = true;
            break;
        } catch (...) {
            Sleep(200);
        }
    }

    if (!ready) {
        TerminateProcess(sei.hProcess, 0);
        CloseHandle(sei.hProcess);
        out.success = false;
        out.error = "DevTools endpoint not ready";
        return out;
    }

    {
        std::lock_guard<std::mutex> lock(mu_);
        Session s;
        s.processHandle = (void*)sei.hProcess;
        s.pid = out.pid;
        s.port = out.port;
        s.app = wApp;
        s.userDataDir = wUserData;
        sessions_[out.session_id] = s;
    }

    out.success = true;
    return out;
}

bool BrowserService::close(const std::string& session_id) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return false;
        }
        s = it->second;
        sessions_.erase(it);
    }

    HANDLE h = (HANDLE)s.processHandle;
    if (h) {
        // Try graceful close first.
        TerminateProcess(h, 0);
        CloseHandle(h);
    }
    return true;
}

BrowserService::NewTabResult BrowserService::newTab(const std::string& session_id) {
    NewTabResult out;
    Session s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            out.success = false;
            out.error = "Unknown session_id";
            return out;
        }
        s = it->second;
    }

    try {
        // Chrome versions differ: /json/new may return 405. Try it first for compatibility.
        try {
            std::string body = HttpGetLocalhostJson(s.port, L"/json/new");
            auto j = nlohmann::json::parse(body);
            out.target_id = j.value("id", "");
            out.websocket_url = j.value("webSocketDebuggerUrl", "");
            if (!out.target_id.empty() && !out.websocket_url.empty()) {
                out.success = true;
                return out;
            }
        } catch (...) {
            // fall through to CDP fallback
        }

        // Fallback: create target via CDP on the browser websocket.
        std::string browserWs = ResolveBrowserWebSocketUrl(s.port);
        nlohmann::json params;
        params["url"] = "about:blank";
        auto resp = SendCdpCommand(browserWs, "Target.createTarget", params);
        std::string targetId;
        if (resp.contains("result") && resp["result"].is_object()) {
            targetId = resp["result"].value("targetId", "");
        }
        if (targetId.empty()) {
            out.success = false;
            out.error = "Failed to create tab (Target.createTarget)";
            return out;
        }

        // Wait for the new target to be discoverable via /json/list.
        std::string wsUrl;
        for (int i = 0; i < 50; i++) {
            try {
                wsUrl = ResolveTargetWebSocketUrl(s.port, targetId);
                break;
            } catch (...) {
                Sleep(100);
            }
        }
        if (wsUrl.empty()) {
            out.success = false;
            out.error = "Failed to resolve websocket url for new tab";
            return out;
        }

        out.target_id = targetId;
        out.websocket_url = wsUrl;
        out.success = true;
        return out;
    } catch (const std::exception& e) {
        out.success = false;
        out.error = e.what();
        return out;
    }
}

bool BrowserService::navigate(const std::string& session_id,
                              const std::string& target_id,
                              const std::string& url,
                              std::string* outError) {
    Session s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            if (outError) *outError = "Unknown session_id";
            return false;
        }
        s = it->second;
    }

    try {
        std::string wsUrl = ResolveTargetWebSocketUrl(s.port, target_id);
        (void)SendCdpCommand(wsUrl, "Page.enable", nlohmann::json::object());
        nlohmann::json params;
        params["url"] = url;
        (void)SendCdpCommand(wsUrl, "Page.navigate", params);
        return true;
    } catch (const std::exception& e) {
        if (outError) *outError = e.what();
        return false;
    }
}

BrowserService::EvalResult BrowserService::eval(const std::string& session_id,
                                                const std::string& target_id,
                                                const std::string& expression,
                                                bool awaitPromise) {
    EvalResult out;
    Session s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            out.success = false;
            out.error = "Unknown session_id";
            return out;
        }
        s = it->second;
    }

    try {
        std::string wsUrl = ResolveTargetWebSocketUrl(s.port, target_id);
        (void)SendCdpCommand(wsUrl, "Runtime.enable", nlohmann::json::object());
        nlohmann::json params;
        params["expression"] = expression;
        params["returnByValue"] = true;
        params["awaitPromise"] = awaitPromise;
        auto resp = SendCdpCommand(wsUrl, "Runtime.evaluate", params);
        if (resp.contains("error")) {
            out.success = false;
            out.error = resp["error"].dump();
            return out;
        }
        out.value = resp.value("result", nlohmann::json::object());
        out.success = true;
        return out;
    } catch (const std::exception& e) {
        out.success = false;
        out.error = e.what();
        return out;
    }
}

BrowserService::ScreenshotResult BrowserService::screenshotPngToFile(const std::string& session_id,
                                                                     const std::string& target_id) {
    ScreenshotResult out;
    Session s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            out.success = false;
            out.error = "Unknown session_id";
            return out;
        }
        s = it->second;
    }

    try {
        std::string wsUrl = ResolveTargetWebSocketUrl(s.port, target_id);
        (void)SendCdpCommand(wsUrl, "Page.enable", nlohmann::json::object());

        nlohmann::json params;
        params["format"] = "png";
        auto resp = SendCdpCommand(wsUrl, "Page.captureScreenshot", params);
        if (!resp.contains("result") || !resp["result"].is_object()) {
            out.success = false;
            out.error = "Invalid captureScreenshot response";
            return out;
        }
        std::string dataB64 = resp["result"].value("data", "");
        if (dataB64.empty()) {
            out.success = false;
            out.error = "Empty screenshot data";
            return out;
        }

        auto bytes = Base64DecodeToBytes(dataB64);
        std::string path = EnsureScreenshotsDirAndName();
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) {
            out.success = false;
            out.error = "Failed to write screenshot file";
            return out;
        }
        f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
        f.close();

        out.success = true;
        out.path = path;
        out.bytes = bytes.size();
        return out;
    } catch (const std::exception& e) {
        out.success = false;
        out.error = e.what();
        return out;
    }
}

BrowserService::OpenUrlResult BrowserService::openUrl(const std::string& url,
                                                       const std::string& app,
                                                       bool headless,
                                                       const std::string& session_id) {
    OpenUrlResult out;

    if (url.empty()) {
        out.success = false;
        out.error = "URL is required";
        return out;
    }

    std::string useSessionId = session_id;

    // Check if session exists
    bool sessionExists = false;
    if (!useSessionId.empty()) {
        std::lock_guard<std::mutex> lock(mu_);
        sessionExists = (sessions_.find(useSessionId) != sessions_.end());
    }

    // If no session or session doesn't exist, launch new browser
    if (useSessionId.empty() || !sessionExists) {
        std::string launchApp = app;
        if (launchApp.empty()) {
            // Default to Chrome
            launchApp = "chrome";
        }

        auto launchResult = launch(launchApp, headless, "", {});
        if (!launchResult.success) {
            out.success = false;
            out.error = "Failed to launch browser: " + launchResult.error;
            return out;
        }

        useSessionId = launchResult.session_id;
        out.session_id = launchResult.session_id;
        out.port = launchResult.port;
        out.pid = launchResult.pid;
    } else {
        // Use existing session
        out.session_id = useSessionId;
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(useSessionId);
        if (it != sessions_.end()) {
            out.port = it->second.port;
            out.pid = it->second.pid;
        }
    }

    // Create new tab
    auto tabResult = newTab(useSessionId);
    if (!tabResult.success) {
        out.success = false;
        out.error = "Failed to create new tab: " + tabResult.error;
        return out;
    }

    out.target_id = tabResult.target_id;

    // Navigate to URL
    std::string navError;
    bool navSuccess = navigate(useSessionId, tabResult.target_id, url, &navError);
    if (!navSuccess) {
        out.success = false;
        out.error = "Failed to navigate: " + navError;
        return out;
    }

    out.success = true;
    return out;
}

std::string BrowserService::getDevToolsUrl(const std::string& session_id,
                                           const std::string& target_id,
                                           std::string* outError) {
    if (outError) outError->clear();
    if (session_id.empty() || target_id.empty()) {
        if (outError) *outError = "session_id and target_id are required";
        return "";
    }

    Session s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            if (outError) *outError = "Unknown session_id";
            return "";
        }
        s = it->second;
    }

    try {
        auto target = FindTargetInfoById(s.port, target_id);
        std::string url = BuildDevToolsUrlFromTarget(s.port, target);
        if (url.empty()) {
            if (outError) *outError = "DevTools URL not available";
            return "";
        }
        return url;
    } catch (const std::exception& e) {
        if (outError) *outError = e.what();
        return "";
    }
}
