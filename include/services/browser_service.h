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
#ifndef CLAWDESK_BROWSER_SERVICE_H
#define CLAWDESK_BROWSER_SERVICE_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include <nlohmann/json.hpp>

// Minimal CDP (Chrome DevTools Protocol) controller for Edge/Chrome using
// remote-debugging HTTP discovery + WinHTTP WebSocket.
class BrowserService {
public:
    struct LaunchResult {
        bool success = false;
        std::string session_id;
        int port = 0;
        uint32_t pid = 0;
        std::string error;
    };

    struct NewTabResult {
        bool success = false;
        std::string target_id;
        std::string websocket_url;
        std::string error;
    };

    struct EvalResult {
        bool success = false;
        nlohmann::json value;
        std::string error;
    };

    struct ScreenshotResult {
        bool success = false;
        std::string path;
        size_t bytes = 0;
        std::string error;
    };

    struct OpenUrlResult {
        bool success = false;
        std::string session_id;
        std::string target_id;
        int port = 0;
        uint32_t pid = 0;
        std::string error;
    };

    BrowserService() = default;
    ~BrowserService();

    LaunchResult launch(const std::string& app,
                        bool headless,
                        const std::string& user_data_dir,
                        const std::vector<std::string>& additional_args);

    bool close(const std::string& session_id);

    NewTabResult newTab(const std::string& session_id);
    bool navigate(const std::string& session_id, const std::string& target_id, const std::string& url, std::string* outError);

    EvalResult eval(const std::string& session_id, const std::string& target_id, const std::string& expression, bool awaitPromise);

    ScreenshotResult screenshotPngToFile(const std::string& session_id,
                                         const std::string& target_id);

    // Returns a URL like:
    //   http://127.0.0.1:<port>/devtools/inspector.html?ws=127.0.0.1:<port>/devtools/page/<id>
    // Opening it shows DevTools (Console/Network/etc.) for the target.
    std::string getDevToolsUrl(const std::string& session_id,
                               const std::string& target_id,
                               std::string* outError);

    // Convenience method: launch browser (if needed) and open URL in new tab
    OpenUrlResult openUrl(const std::string& url,
                          const std::string& app = "",
                          bool headless = false,
                          const std::string& session_id = "");

private:
    struct Session {
        void* processHandle = nullptr;
        uint32_t pid = 0;
        int port = 0;
        std::wstring app;
        std::wstring userDataDir;
    };

    std::mutex mu_;
    std::map<std::string, Session> sessions_;

    static std::string generateSessionId();
    static int pickFreePort();
};

#endif // CLAWDESK_BROWSER_SERVICE_H
