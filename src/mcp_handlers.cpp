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
#include "mcp_handlers.h"
#include "app_globals.h"
#include <algorithm>
#include <cctype>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "mcp/tool_registry.h"
#include "services/file_service.h"
#include "services/process_service.h"
#include "services/file_operation_service.h"
#include "services/power_service.h"
#include "services/clipboard_service.h"
#include "services/window_service.h"
#include "services/app_service.h"
#include "services/command_service.h"
#include "services/screenshot_service.h"
#include "services/browser_service.h"
#include "policy/policy_guard.h"
#include "support/config_manager.h"
#include "support/license_manager.h"
#include "support/audit_logger.h"

// ToolRegistry 在全局 namespace

nlohmann::json MakeTextContent(const std::string& text, bool isError) {
    nlohmann::json response;
    response["content"] = nlohmann::json::array();
    response["content"].push_back({{"type", "text"}, {"text", text}});
    response["isError"] = isError;
    return response;
}

std::string DumpMcpResponse(const nlohmann::json& response) {
    return response.dump();
}

void RegisterMcpTools() {
    auto& registry = ToolRegistry::getInstance();

    registry.registerTool("read_file", {
        "read_file",
        "Read file content",
        clawdesk::RiskLevel::Low,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}}},
            {"required", {"path"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileService) {
                return MakeTextContent("Error: FileService not initialized", true);
            }
            if (!args.contains("path")) {
                return MakeTextContent("Error: Path is required", true);
            }
            try {
                std::string content = g_fileService->readTextFile(args["path"].get<std::string>());
                if (g_policyGuard) g_policyGuard->incrementUsageCount("read_file");
                return MakeTextContent(content, false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("write_file", {
        "write_file",
        "Write text content to a file (within allowed_dirs)",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"content", {{"type", "string"}}},
                {"overwrite", {{"type", "boolean"}}},
                {"line_endings", {{"type", "string"}, {"description", "auto|lf|crlf"}}}
            }},
            {"required", {"path", "content"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileService) {
                return MakeTextContent("Error: FileService not initialized", true);
            }
            std::string path = args.value("path", "");
            if (path.empty()) {
                return MakeTextContent("Error: path is required", true);
            }
            if (!args.contains("content") || !args["content"].is_string()) {
                return MakeTextContent("Error: content is required", true);
            }
            std::string content = args["content"].get<std::string>();
            bool overwrite = args.value("overwrite", false);
            std::string lineEndings = args.value("line_endings", "auto");

            // If writing a batch file, default to CRLF for best compatibility.
            std::string lowerPath = path;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lineEndings == "auto" &&
                (lowerPath.size() >= 4 &&
                 (lowerPath.rfind(".bat") == lowerPath.size() - 4 ||
                  lowerPath.rfind(".cmd") == lowerPath.size() - 4))) {
                lineEndings = "crlf";
            }

            try {
                g_fileService->writeTextFile(path, content, overwrite, lineEndings);
                if (g_policyGuard) g_policyGuard->incrementUsageCount("write_file");
                nlohmann::json payload{{"success", true}, {"path", path}, {"bytes", content.size()}};
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("run_bat", {
        "run_bat",
        "Run a .bat/.cmd file from C:\\Temp only (safer than execute_command)",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"visible", {{"type", "boolean"}, {"description", "Show a console window (default true). Use true for scripts with pause."}}},
                {"wait_ms", {{"type", "number"}, {"description", "Wait for completion up to N ms (0 = don't wait). Default 0."}}}
            }},
            {"required", {"path"}}
        },
        [](const nlohmann::json& args) {
            std::string path = args.value("path", "");
            if (path.empty()) {
                return MakeTextContent("Error: path is required", true);
            }

            // Canonicalize to defeat .. traversal
            char fullBuf[MAX_PATH] = {0};
            DWORD n = GetFullPathNameA(path.c_str(), MAX_PATH, fullBuf, NULL);
            if (n == 0 || n >= MAX_PATH) {
                return MakeTextContent("Error: invalid path", true);
            }
            std::string fullPath(fullBuf);

            auto normalizePath = [](std::string p) -> std::string {
                std::replace(p.begin(), p.end(), '/', '\\');
                std::transform(p.begin(), p.end(), p.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return p;
            };

            std::string fullLower = normalizePath(fullPath);

            // Only allow under C:\Temp
            std::string allowedPrefix = "c:\\temp\\";
            if (fullLower.rfind(allowedPrefix, 0) != 0) {
                return MakeTextContent("Error: run_bat only allows files under C:\\Temp\\", true);
            }

            // Only .bat/.cmd
            size_t dot = fullLower.find_last_of('.');
            std::string ext = (dot == std::string::npos) ? "" : fullLower.substr(dot);
            if (!(ext == ".bat" || ext == ".cmd")) {
                return MakeTextContent("Error: only .bat/.cmd allowed", true);
            }

            // Reject quotes to avoid cmd.exe quoting tricks
            if (fullPath.find('\"') != std::string::npos) {
                return MakeTextContent("Error: invalid path", true);
            }

            DWORD attrs = GetFileAttributesA(fullPath.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                return MakeTextContent("Error: file not found", true);
            }

            // PolicyGuard path checks + confirmations
            if (g_policyGuard) {
                nlohmann::json policyArgs;
                policyArgs["path"] = fullPath;
                auto decision = g_policyGuard->evaluateToolCall("run_bat", policyArgs);
                if (!decision.allowed) {
                    return MakeTextContent(std::string("Error: ") + decision.reason, true);
                }
            }

            bool visible = args.value("visible", true);
            int waitMs = args.value("wait_ms", 0);
            if (waitMs < 0) waitMs = 0;
            if (waitMs > 600000) waitMs = 600000; // cap at 10 minutes

            // cmd /c ""C:\Temp\script.bat""
            std::string cmdLine = "cmd.exe /c \"\"" + fullPath + "\"\"";
            std::vector<char> buf(cmdLine.begin(), cmdLine.end());
            buf.push_back('\0');

            STARTUPINFOA si{};
            PROCESS_INFORMATION pi{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = visible ? SW_SHOWNORMAL : SW_HIDE;

            DWORD flags = 0;
            if (!visible) {
                flags |= CREATE_NO_WINDOW;
            }

            BOOL ok = CreateProcessA(NULL,
                                     buf.data(),
                                     NULL,
                                     NULL,
                                     FALSE,
                                     flags,
                                     NULL,
                                     "C:\\Temp",
                                     &si,
                                     &pi);
            if (!ok) {
                return MakeTextContent(std::string("Error: failed to start process (") + std::to_string(GetLastError()) + ")", true);
            }

            DWORD exitCode = 0;
            bool waited = false;
            bool timedOut = false;
            if (waitMs > 0) {
                waited = true;
                DWORD wr = WaitForSingleObject(pi.hProcess, (DWORD)waitMs);
                if (wr == WAIT_TIMEOUT) {
                    timedOut = true;
                } else {
                    GetExitCodeProcess(pi.hProcess, &exitCode);
                }
            }

            nlohmann::json payload{
                {"success", true},
                {"pid", (uint32_t)pi.dwProcessId},
                {"waited", waited},
                {"timed_out", timedOut},
                {"exit_code", waited && !timedOut ? (int)exitCode : -1},
                {"path", fullPath}
            };

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            if (g_policyGuard) g_policyGuard->incrementUsageCount("run_bat");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("search_file", {
        "search_file",
        "Search text in file",
        clawdesk::RiskLevel::Low,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}, {"query", {{"type", "string"}}}}},
            {"required", {"path", "query"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileService) {
                return MakeTextContent("Error: FileService not initialized", true);
            }
            if (!args.contains("path") || !args.contains("query")) {
                return MakeTextContent("Error: Path and query are required", true);
            }
            try {
                auto matches = g_fileService->searchTextInFile(
                    args["path"].get<std::string>(),
                    args["query"].get<std::string>()
                );
                nlohmann::json payload;
                payload["path"] = args["path"];
                payload["query"] = args["query"];
                payload["matches"] = nlohmann::json::array();
                for (const auto& match : matches) {
                    payload["matches"].push_back({{"line", match.line}, {"text", match.text}});
                }
                if (g_policyGuard) g_policyGuard->incrementUsageCount("search_file");
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("search_files", {
        "search_files",
        "Search files by name, metadata, and optional content",
        clawdesk::RiskLevel::Low,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"name_query", {{"type", "string"}}},
                {"content_query", {{"type", "string"}}},
                {"exts", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                {"days", {{"type", "number"}}},
                {"min_size", {{"type", "number"}}},
                {"max_size", {{"type", "number"}}},
                {"max", {{"type", "number"}}}
            }}
        },
        [](const nlohmann::json& args) {
            if (!g_fileService) {
                return MakeTextContent("Error: FileService not initialized", true);
            }
            FindFilesParams params{};
            params.query = args.value("name_query", "");
            params.days = args.value("days", 0);
            params.max = args.value("max", 100);
            if (params.max <= 0) {
                params.max = 100;
            }
            if (args.contains("exts") && args["exts"].is_array()) {
                for (const auto& ext : args["exts"]) {
                    if (ext.is_string()) {
                        params.exts.push_back(ext.get<std::string>());
                    }
                }
            }
            std::string path = args.value("path", "");
            std::vector<FileInfo> files;
            if (!path.empty()) {
                files = g_fileService->findFilesInPath(path, params);
            } else {
                files = g_fileService->findFiles(params);
            }

            int64_t minSize = args.value("min_size", static_cast<int64_t>(-1));
            int64_t maxSize = args.value("max_size", static_cast<int64_t>(-1));
            std::string contentQuery = args.value("content_query", "");

            nlohmann::json payload = nlohmann::json::array();
            for (const auto& file : files) {
                if (minSize >= 0 && file.size < minSize) {
                    continue;
                }
                if (maxSize >= 0 && file.size > maxSize) {
                    continue;
                }

                nlohmann::json entry;
                entry["path"] = file.path;
                entry["size"] = file.size;
                entry["modified"] = file.modified;
                entry["extension"] = file.extension;

                if (!contentQuery.empty()) {
                    try {
                        auto matches = g_fileService->searchTextInFile(file.path, contentQuery);
                        if (matches.empty()) {
                            continue;
                        }
                        nlohmann::json matchList = nlohmann::json::array();
                        size_t limit = std::min<size_t>(matches.size(), 5);
                        for (size_t i = 0; i < limit; ++i) {
                            matchList.push_back({
                                {"line", matches[i].line},
                                {"text", matches[i].text}
                            });
                        }
                        entry["content_match_count"] = matches.size();
                        entry["content_matches"] = matchList;
                    } catch (const std::exception&) {
                        continue;
                    }
                }

                payload.push_back(entry);
                if (params.max > 0 && static_cast<int>(payload.size()) >= params.max) {
                    break;
                }
            }

            if (g_policyGuard) g_policyGuard->incrementUsageCount("search_files");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("list_directory", {
        "list_directory",
        "List directory",
        clawdesk::RiskLevel::Low,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}}},
            {"required", {"path"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileService) {
                return MakeTextContent("Error: FileService not initialized", true);
            }
            if (!args.contains("path")) {
                return MakeTextContent("Error: Path is required", true);
            }
            try {
                auto entries = g_fileService->listDirectory(args["path"].get<std::string>());
                nlohmann::json payload = nlohmann::json::array();
                for (const auto& entry : entries) {
                    payload.push_back({
                        {"name", entry.name},
                        {"type", entry.type},
                        {"size", entry.size},
                        {"modified", entry.modified}
                    });
                }
                if (g_policyGuard) g_policyGuard->incrementUsageCount("list_directory");
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("get_clipboard", {
        "get_clipboard",
        "Get clipboard",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](const nlohmann::json&) {
            if (!g_clipboardService) {
                return MakeTextContent("Error: ClipboardService not initialized", true);
            }
            try {
                std::string content = g_clipboardService->readText();
                if (g_policyGuard) g_policyGuard->incrementUsageCount("get_clipboard");
                return MakeTextContent(content, false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("set_clipboard", {
        "set_clipboard",
        "Set clipboard",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"content", {{"type", "string"}}}}},
            {"required", {"content"}}
        },
        [](const nlohmann::json& args) {
            if (!g_clipboardService) {
                return MakeTextContent("Error: ClipboardService not initialized", true);
            }
            if (!args.contains("content")) {
                return MakeTextContent("Error: Content is required", true);
            }
            try {
                g_clipboardService->writeText(args["content"].get<std::string>());
                if (g_policyGuard) g_policyGuard->incrementUsageCount("set_clipboard");
                return MakeTextContent("Clipboard updated", false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("take_screenshot", {
        "take_screenshot",
        "Take screenshot",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"format", {{"type", "string"}}}}}
        },
        [](const nlohmann::json&) {
            if (!g_screenshotService) {
                return MakeTextContent("Error: ScreenshotService not initialized", true);
            }
            try {
                auto shot = g_screenshotService->captureFullScreen();
                nlohmann::json payload;
                payload["path"] = shot.path;
                payload["width"] = shot.width;
                payload["height"] = shot.height;
                payload["created_at"] = shot.created_at;
                if (g_policyGuard) g_policyGuard->incrementUsageCount("take_screenshot");
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("take_screenshot_window", {
        "take_screenshot_window",
        "Take screenshot of a window by title",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"title", {{"type", "string"}}}}},
            {"required", {"title"}}
        },
        [](const nlohmann::json& args) {
            if (!g_screenshotService) {
                return MakeTextContent("Error: ScreenshotService not initialized", true);
            }
            if (!args.contains("title")) {
                return MakeTextContent("Error: Title is required", true);
            }
            try {
                auto shot = g_screenshotService->captureWindowByTitle(args["title"].get<std::string>());
                nlohmann::json payload;
                payload["path"] = shot.path;
                payload["width"] = shot.width;
                payload["height"] = shot.height;
                payload["created_at"] = shot.created_at;
                if (g_policyGuard) g_policyGuard->incrementUsageCount("take_screenshot_window");
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("take_screenshot_region", {
        "take_screenshot_region",
        "Take screenshot of a region",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"x", {{"type", "number"}}},
                {"y", {{"type", "number"}}},
                {"width", {{"type", "number"}}},
                {"height", {{"type", "number"}}}
            }},
            {"required", {"x", "y", "width", "height"}}
        },
        [](const nlohmann::json& args) {
            if (!g_screenshotService) {
                return MakeTextContent("Error: ScreenshotService not initialized", true);
            }
            if (!args.contains("x") || !args.contains("y") ||
                !args.contains("width") || !args.contains("height")) {
                return MakeTextContent("Error: x, y, width, height are required", true);
            }
            try {
                int x = args["x"].get<int>();
                int y = args["y"].get<int>();
                int width = args["width"].get<int>();
                int height = args["height"].get<int>();
                auto shot = g_screenshotService->captureRegion(x, y, width, height);
                nlohmann::json payload;
                payload["path"] = shot.path;
                payload["width"] = shot.width;
                payload["height"] = shot.height;
                payload["created_at"] = shot.created_at;
                if (g_policyGuard) g_policyGuard->incrementUsageCount("take_screenshot_region");
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("list_windows", {
        "list_windows",
        "List windows",
        clawdesk::RiskLevel::Low,
        false,
        nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](const nlohmann::json&) {
            if (!g_windowService) {
                return MakeTextContent("Error: WindowService not initialized", true);
            }
            auto windows = g_windowService->listVisibleWindows();
            nlohmann::json payload = nlohmann::json::array();
            for (const auto& win : windows) {
                payload.push_back({
                    {"title", win.title},
                    {"process_name", win.processName},
                    {"pid", win.pid}
                });
            }
            if (g_policyGuard) g_policyGuard->incrementUsageCount("list_windows");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("focus_window", {
        "focus_window",
        "Show and focus a window by title or process name",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"title", {{"type", "string"}}},
                {"process_name", {{"type", "string"}}}
            }},
            {"minProperties", 1}
        },
        [](const nlohmann::json& args) {
            if (!g_windowService) {
                return MakeTextContent("Error: WindowService not initialized", true);
            }
            std::string title = args.value("title", "");
            std::string processName = args.value("process_name", "");
            if (title.empty() && processName.empty()) {
                return MakeTextContent("Error: title or process_name is required", true);
            }
            WindowInfo info{};
            bool ok = g_windowService->focusWindow(title, processName, &info);
            if (!ok) {
                return MakeTextContent("Error: window not found or could not be focused", true);
            }
            nlohmann::json payload;
            payload["title"] = info.title;
            payload["process_name"] = info.processName;
            payload["pid"] = info.pid;
            if (g_policyGuard) g_policyGuard->incrementUsageCount("focus_window");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("set_window_topmost", {
        "set_window_topmost",
        "Set a window always-on-top or normal",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"title", {{"type", "string"}}},
                {"process_name", {{"type", "string"}}},
                {"pid", {{"type", "number"}}},
                {"topmost", {{"type", "boolean"}}}
            }},
            {"minProperties", 1}
        },
        [](const nlohmann::json& args) {
            if (!g_windowService) {
                return MakeTextContent("Error: WindowService not initialized", true);
            }
            std::string title = args.value("title", "");
            std::string processName = args.value("process_name", "");
            DWORD pid = args.value("pid", 0);
            HWND hwnd = NULL;
            WindowInfo info{};
            if (!g_windowService->findWindow(title, processName, pid, &hwnd, &info)) {
                return MakeTextContent("Error: target window not found", true);
            }
            bool topmost = args.value("topmost", true);
            HWND insertAfter = topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
            if (!SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)) {
                return MakeTextContent("Error: failed to set topmost", true);
            }
            if (g_policyGuard) g_policyGuard->incrementUsageCount("set_window_topmost");
            nlohmann::json payload;
            payload["title"] = info.title;
            payload["process_name"] = info.processName;
            payload["pid"] = info.pid;
            payload["topmost"] = topmost;
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("set_window_state", {
        "set_window_state",
        "Set window state: minimize, maximize, restore",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"title", {{"type", "string"}}},
                {"process_name", {{"type", "string"}}},
                {"pid", {{"type", "number"}}},
                {"action", {{"type", "string"}}}
            }},
            {"required", {"action"}}
        },
        [](const nlohmann::json& args) {
            if (!g_windowService) {
                return MakeTextContent("Error: WindowService not initialized", true);
            }
            std::string action = args.value("action", "");
            if (action.empty()) {
                return MakeTextContent("Error: action is required", true);
            }
            std::string title = args.value("title", "");
            std::string processName = args.value("process_name", "");
            DWORD pid = args.value("pid", 0);
            HWND hwnd = NULL;
            WindowInfo info{};
            if (!g_windowService->findWindow(title, processName, pid, &hwnd, &info)) {
                return MakeTextContent("Error: target window not found", true);
            }
            std::string actionLower = action;
            std::transform(actionLower.begin(), actionLower.end(), actionLower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            int cmd = 0;
            if (actionLower == "minimize") {
                cmd = SW_MINIMIZE;
            } else if (actionLower == "maximize") {
                cmd = SW_MAXIMIZE;
            } else if (actionLower == "restore") {
                cmd = SW_RESTORE;
            } else {
                return MakeTextContent("Error: invalid action", true);
            }
            ShowWindow(hwnd, cmd);
            if (g_policyGuard) g_policyGuard->incrementUsageCount("set_window_state");
            nlohmann::json payload;
            payload["title"] = info.title;
            payload["process_name"] = info.processName;
            payload["pid"] = info.pid;
            payload["action"] = actionLower;
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("send_hotkey", {
        "send_hotkey",
        "Send a keyboard shortcut (hotkey). Optional target window by title or process name",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"hotkey", {{"type", "string"}}},
                {"title", {{"type", "string"}}},
                {"process_name", {{"type", "string"}}}
            }},
            {"required", {"hotkey"}}
        },
        [](const nlohmann::json& args) {
            if (!args.contains("hotkey")) {
                return MakeTextContent("Error: hotkey is required", true);
            }
            std::string title = args.value("title", "");
            std::string processName = args.value("process_name", "");
            if (!title.empty() || !processName.empty()) {
                if (!g_windowService) {
                    return MakeTextContent("Error: WindowService not initialized", true);
                }
                WindowInfo info{};
                if (!g_windowService->focusWindow(title, processName, &info)) {
                    return MakeTextContent("Error: target window not found or could not be focused", true);
                }
                Sleep(200);
            }
            std::string hotkey = args["hotkey"].get<std::string>();
            if (!SendHotkey(hotkey)) {
                return MakeTextContent("Error: failed to send hotkey", true);
            }
            if (g_policyGuard) g_policyGuard->incrementUsageCount("send_hotkey");
            return MakeTextContent("Hotkey sent", false);
        }
    });

    registry.registerTool("list_processes", {
        "list_processes",
        "List processes",
        clawdesk::RiskLevel::Low,
        false,
        nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](const nlohmann::json&) {
            std::string list = GetProcessList();
            if (g_policyGuard) g_policyGuard->incrementUsageCount("list_processes");
            return MakeTextContent(list, false);
        }
    });

    registry.registerTool("execute_command", {
        "execute_command",
        "Execute command",
        clawdesk::RiskLevel::Critical,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"command", {{"type", "string"}}}}},
            {"required", {"command"}}
        },
        [](const nlohmann::json& args) {
            if (!g_commandService) {
                return MakeTextContent("Error: CommandService not initialized", true);
            }
            if (!args.contains("command")) {
                return MakeTextContent("Error: Command is required", true);
            }
            try {
                std::string command = args["command"].get<std::string>();
                std::string trimmed = command;
                trimmed.erase(trimmed.begin(),
                              std::find_if(trimmed.begin(), trimmed.end(),
                                           [](unsigned char c) { return !std::isspace(c); }));
                trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(),
                                           [](unsigned char c) { return !std::isspace(c); }).base(),
                              trimmed.end());
                std::string lower = trimmed;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lower.empty() || lower == "/" || lower == "help") {
                    return MakeTextContent(BuildHelpJson(), false);
                }
                auto cmdResult = g_commandService->executeCommand(command, {});
                nlohmann::json payload;
                payload["stdout"] = cmdResult.stdoutText;
                payload["stderr"] = cmdResult.stderrText;
                payload["exit_code"] = cmdResult.exitCode;
                payload["timed_out"] = cmdResult.timedOut;
                if (g_policyGuard) g_policyGuard->incrementUsageCount("execute_command");
                return MakeTextContent(payload.dump(), false);
            } catch (const std::exception& e) {
                return MakeTextContent(std::string("Error: ") + e.what(), true);
            }
        }
    });

    registry.registerTool("kill_process", {
        "kill_process",
        "Terminate a process by PID",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"pid", {{"type", "number"}}}, {"force", {{"type", "boolean"}}}}},
            {"required", {"pid"}}
        },
        [](const nlohmann::json& args) {
            if (!g_processService) {
                return MakeTextContent("Error: ProcessService not initialized", true);
            }
            if (!args.contains("pid")) {
                return MakeTextContent("Error: PID is required", true);
            }
            DWORD pid = args["pid"].get<DWORD>();
            bool force = args.value("force", false);
            auto result = g_processService->killProcess(pid, force);
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"pid", result.pid},
                {"process_name", result.processName},
                {"forced", result.forced}
            };
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("set_process_priority", {
        "set_process_priority",
        "Set process priority",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"pid", {{"type", "number"}}}, {"priority", {{"type", "string"}}}}},
            {"required", {"pid", "priority"}}
        },
        [](const nlohmann::json& args) {
            if (!g_processService) {
                return MakeTextContent("Error: ProcessService not initialized", true);
            }
            if (!args.contains("pid") || !args.contains("priority")) {
                return MakeTextContent("Error: PID and priority are required", true);
            }
            DWORD pid = args["pid"].get<DWORD>();
            std::string priorityStr = args["priority"].get<std::string>();
            ProcessPriority priority = ProcessPriority::Normal;
            if (priorityStr == "idle") priority = ProcessPriority::Idle;
            else if (priorityStr == "below_normal") priority = ProcessPriority::BelowNormal;
            else if (priorityStr == "normal") priority = ProcessPriority::Normal;
            else if (priorityStr == "above_normal") priority = ProcessPriority::AboveNormal;
            else if (priorityStr == "high") priority = ProcessPriority::High;
            else if (priorityStr == "realtime") priority = ProcessPriority::Realtime;
            auto result = g_processService->setProcessPriority(pid, priority);
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"pid", result.pid},
                {"old_priority", ProcessService::priorityToString(result.oldPriority)},
                {"new_priority", ProcessService::priorityToString(result.newPriority)}
            };
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("delete_file", {
        "delete_file",
        "Delete a file or directory",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}, {"recursive", {{"type", "boolean"}}}}},
            {"required", {"path"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileOperationService) {
                return MakeTextContent("Error: FileOperationService not initialized", true);
            }
            if (!args.contains("path")) {
                return MakeTextContent("Error: Path is required", true);
            }
            auto result = g_fileOperationService->deleteFile(args["path"].get<std::string>(),
                                                             args.value("recursive", false));
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{{"success", true}, {"path", result.path}, {"type", result.type}};
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("copy_file", {
        "copy_file",
        "Copy a file or directory",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"source", {{"type", "string"}}}, {"destination", {{"type", "string"}}}, {"overwrite", {{"type", "boolean"}}}}},
            {"required", {"source", "destination"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileOperationService) {
                return MakeTextContent("Error: FileOperationService not initialized", true);
            }
            if (!args.contains("source") || !args.contains("destination")) {
                return MakeTextContent("Error: Source and destination are required", true);
            }
            auto result = g_fileOperationService->copyFile(
                args["source"].get<std::string>(),
                args["destination"].get<std::string>(),
                args.value("overwrite", false)
            );
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"source", result.source},
                {"destination", result.destination},
                {"size", result.size}
            };
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("move_file", {
        "move_file",
        "Move or rename a file",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"source", {{"type", "string"}}}, {"destination", {{"type", "string"}}}}},
            {"required", {"source", "destination"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileOperationService) {
                return MakeTextContent("Error: FileOperationService not initialized", true);
            }
            if (!args.contains("source") || !args.contains("destination")) {
                return MakeTextContent("Error: Source and destination are required", true);
            }
            auto result = g_fileOperationService->moveFile(
                args["source"].get<std::string>(),
                args["destination"].get<std::string>()
            );
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"source", result.source},
                {"destination", result.destination}
            };
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("create_directory", {
        "create_directory",
        "Create a directory",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}, {"recursive", {{"type", "boolean"}}}}},
            {"required", {"path"}}
        },
        [](const nlohmann::json& args) {
            if (!g_fileOperationService) {
                return MakeTextContent("Error: FileOperationService not initialized", true);
            }
            if (!args.contains("path")) {
                return MakeTextContent("Error: Path is required", true);
            }
            auto result = g_fileOperationService->createDirectory(
                args["path"].get<std::string>(),
                args.value("recursive", false)
            );
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{{"success", true}, {"path", result.path}};
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("shutdown_system", {
        "shutdown_system",
        "Shutdown, reboot, hibernate or sleep the system",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"action", {{"type", "string"}}},
                {"delay", {{"type", "number"}}},
                {"force", {{"type", "boolean"}}},
                {"message", {{"type", "string"}}}
            }},
            {"required", {"action"}}
        },
        [](const nlohmann::json& args) {
            if (!g_powerService) {
                return MakeTextContent("Error: PowerService not initialized", true);
            }
            if (!args.contains("action")) {
                return MakeTextContent("Error: Action is required", true);
            }
            std::string actionStr = args["action"].get<std::string>();
            PowerAction action = PowerAction::Shutdown;
            if (actionStr == "reboot") action = PowerAction::Reboot;
            else if (actionStr == "hibernate") action = PowerAction::Hibernate;
            else if (actionStr == "sleep") action = PowerAction::Sleep;
            int delay = args.value("delay", 0);
            bool force = args.value("force", false);
            std::string message = args.value("message", "");
            auto result = g_powerService->shutdownSystem(action, delay, force, message);
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"action", actionStr},
                {"delay", result.delay},
                {"scheduled_time", result.scheduledTime}
            };
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("abort_shutdown", {
        "abort_shutdown",
        "Cancel a scheduled shutdown or reboot",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}},
        [](const nlohmann::json&) {
            if (!g_powerService) {
                return MakeTextContent("Error: PowerService not initialized", true);
            }
            auto result = g_powerService->abortShutdown();
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{{"success", true}, {"message", result.message}};
            return MakeTextContent(payload.dump(), false);
        }
    });

    // Browser MCP (CDP over WinHTTP WebSocket)
    registry.registerTool("browser_launch", {
        "browser_launch",
        "Launch Edge/Chrome with remote debugging enabled (CDP)",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"app", {{"type", "string"}, {"description", "Allowed app key or path. Recommended: configure allowed_apps and pass key, e.g. chrome"}}},
                {"headless", {{"type", "boolean"}}},
                {"user_data_dir", {{"type", "string"}}},
                {"additional_args", {{"type", "array"}, {"items", {{"type", "string"}}}}}
            }},
            {"required", {"app"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            if (!args.contains("app") || !args["app"].is_string()) {
                return MakeTextContent("Error: app is required", true);
            }
            std::string app = args["app"].get<std::string>();
            std::string appPathOrName = app;
            if (g_configManager) {
                auto allowed = g_configManager->getAllowedApps();
                auto it = allowed.find(app);
                if (it != allowed.end() && !it->second.empty()) {
                    appPathOrName = it->second;
                }
            }
            bool headless = args.value("headless", false);
            std::string userDataDir = args.value("user_data_dir", "");
            std::vector<std::string> additional;
            if (args.contains("additional_args") && args["additional_args"].is_array()) {
                for (const auto& it : args["additional_args"]) {
                    if (it.is_string()) additional.push_back(it.get<std::string>());
                }
            }
            auto result = g_browserService->launch(appPathOrName, headless, userDataDir, additional);
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"session_id", result.session_id},
                {"port", result.port},
                {"pid", result.pid}
            };
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_launch");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_new_tab", {
        "browser_new_tab",
        "Create a new tab (target) via DevTools /json/new",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"session_id", {{"type", "string"}}}}},
            {"required", {"session_id"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            if (sessionId.empty()) {
                return MakeTextContent("Error: session_id is required", true);
            }
            auto tab = g_browserService->newTab(sessionId);
            if (!tab.success) {
                return MakeTextContent(std::string("Error: ") + tab.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"target_id", tab.target_id},
                {"websocket_url", tab.websocket_url}
            };
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_new_tab");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_navigate", {
        "browser_navigate",
        "Navigate a tab (target) to a URL",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"session_id", {{"type", "string"}}},
                {"target_id", {{"type", "string"}}},
                {"url", {{"type", "string"}}}
            }},
            {"required", {"session_id", "target_id", "url"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            std::string targetId = args.value("target_id", "");
            std::string url = args.value("url", "");
            if (sessionId.empty() || targetId.empty() || url.empty()) {
                return MakeTextContent("Error: session_id, target_id, url are required", true);
            }
            std::string err;
            bool ok = g_browserService->navigate(sessionId, targetId, url, &err);
            if (!ok) {
                return MakeTextContent(std::string("Error: ") + err, true);
            }
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_navigate");
            return MakeTextContent(nlohmann::json{{"success", true}}.dump(), false);
        }
    });

    registry.registerTool("browser_eval", {
        "browser_eval",
        "Evaluate JavaScript in a tab (Runtime.evaluate)",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"session_id", {{"type", "string"}}},
                {"target_id", {{"type", "string"}}},
                {"expression", {{"type", "string"}}},
                {"await_promise", {{"type", "boolean"}}}
            }},
            {"required", {"session_id", "target_id", "expression"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            std::string targetId = args.value("target_id", "");
            std::string expr = args.value("expression", "");
            bool awaitPromise = args.value("await_promise", false);
            if (sessionId.empty() || targetId.empty() || expr.empty()) {
                return MakeTextContent("Error: session_id, target_id, expression are required", true);
            }
            auto res = g_browserService->eval(sessionId, targetId, expr, awaitPromise);
            if (!res.success) {
                return MakeTextContent(std::string("Error: ") + res.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"result", res.value}
            };
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_eval");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_screenshot", {
        "browser_screenshot",
        "Capture a screenshot of the current tab and save it under screenshots/",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"session_id", {{"type", "string"}}},
                {"target_id", {{"type", "string"}}}
            }},
            {"required", {"session_id", "target_id"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            std::string targetId = args.value("target_id", "");
            if (sessionId.empty() || targetId.empty()) {
                return MakeTextContent("Error: session_id and target_id are required", true);
            }
            auto shot = g_browserService->screenshotPngToFile(sessionId, targetId);
            if (!shot.success) {
                return MakeTextContent(std::string("Error: ") + shot.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"path", shot.path},
                {"bytes", shot.bytes}
            };
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_screenshot");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_close", {
        "browser_close",
        "Close a browser session",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"session_id", {{"type", "string"}}}}},
            {"required", {"session_id"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            if (sessionId.empty()) {
                return MakeTextContent("Error: session_id is required", true);
            }
            bool ok = g_browserService->close(sessionId);
            if (!ok) {
                return MakeTextContent("Error: session_id not found", true);
            }
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_close");
            return MakeTextContent(nlohmann::json{{"success", true}}.dump(), false);
        }
    });

    registry.registerTool("browser_open_url", {
        "browser_open_url",
        "Convenience tool: launch browser (if needed) and open URL in new tab",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"url", {{"type", "string"}, {"description", "URL to open"}}},
                {"app", {{"type", "string"}, {"description", "Browser app (optional, defaults to chrome)"}}},
                {"headless", {{"type", "boolean"}, {"description", "Run in headless mode (optional)"}}},
                {"session_id", {{"type", "string"}, {"description", "Reuse existing session (optional)"}}}
            }},
            {"required", {"url"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            if (!args.contains("url") || !args["url"].is_string()) {
                return MakeTextContent("Error: url is required", true);
            }
            std::string url = args["url"].get<std::string>();
            std::string app = args.value("app", "");
            bool headless = args.value("headless", false);
            std::string sessionId = args.value("session_id", "");

            std::string appKey = app.empty() ? "chrome" : app;
            if (g_policyGuard && !g_policyGuard->isAppAllowed(appKey)) {
                return MakeTextContent("Error: App not allowed", true);
            }

            auto result = g_browserService->openUrl(url, app, headless, sessionId);
            if (!result.success) {
                return MakeTextContent(std::string("Error: ") + result.error, true);
            }
            nlohmann::json payload{
                {"success", true},
                {"session_id", result.session_id},
                {"target_id", result.target_id},
                {"port", result.port},
                {"pid", result.pid}
            };
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_open_url");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_fetch_text", {
        "browser_fetch_text",
        "Convenience tool: open a URL in browser and return extracted page text (optionally tweet text)",
        clawdesk::RiskLevel::High,
        true,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"url", {{"type", "string"}, {"description", "URL to open"}}},
                {"app", {{"type", "string"}, {"description", "Browser app (optional, defaults to chrome)"}}},
                {"headless", {{"type", "boolean"}, {"description", "Run in headless mode (optional)"}}},
                {"session_id", {{"type", "string"}, {"description", "Reuse existing session (optional)"}}},
                {"wait_ms", {{"type", "number"}, {"description", "Wait before extraction (optional, default 1000)"}}},
                {"max_chars", {{"type", "number"}, {"description", "Max returned characters (optional, default 8000)"}}},
                {"extract", {{"type", "string"}, {"description", "Extraction mode: body_text | tweet_text (optional, default body_text)"}}}
            }},
            {"required", {"url"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            if (!args.contains("url") || !args["url"].is_string()) {
                return MakeTextContent("Error: url is required", true);
            }
            std::string url = args["url"].get<std::string>();
            std::string app = args.value("app", "");
            bool headless = args.value("headless", false);
            std::string sessionId = args.value("session_id", "");
            int waitMs = (int)args.value("wait_ms", 1000);
            if (waitMs < 0) waitMs = 0;
            if (waitMs > 30000) waitMs = 30000;
            int maxChars = (int)args.value("max_chars", 8000);
            if (maxChars < 256) maxChars = 256;
            if (maxChars > 200000) maxChars = 200000;
            std::string extract = args.value("extract", "body_text");

            std::string appKey = app.empty() ? "chrome" : app;
            if (g_policyGuard && !g_policyGuard->isAppAllowed(appKey)) {
                return MakeTextContent("Error: App not allowed", true);
            }

            auto opened = g_browserService->openUrl(url, app, headless, sessionId);
            if (!opened.success) {
                return MakeTextContent(std::string("Error: ") + opened.error, true);
            }

            if (waitMs > 0) {
                Sleep((DWORD)waitMs);
            }

            // Poll a bit to allow dynamic pages to render.
            std::string js =
                "(async () => {"
                "  const sleep = (ms) => new Promise(r => setTimeout(r, ms));"
                "  const maxChars = " + std::to_string(maxChars) + ";"
                "  const mode = " + nlohmann::json(extract).dump() + ";"
                "  for (let i = 0; i < 40; i++) {"
                "    let text = '';"
                "    if (mode === 'tweet_text') {"
                "      const t = Array.from(document.querySelectorAll('article [data-testid=\"tweetText\"]'))"
                "        .map(e => (e && e.innerText) ? e.innerText.trim() : '')"
                "        .filter(Boolean);"
                "      if (t.length) text = t.join('\\n\\n');"
                "    }"
                "    if (!text) {"
                "      text = (document.body && document.body.innerText) ? document.body.innerText : '';"
                "    }"
                "    text = String(text || '');"
                "    if (text.trim().length > 0) {"
                "      return { ok: true, url: location.href, title: document.title || '', text: text.slice(0, maxChars) };"
                "    }"
                "    await sleep(250);"
                "  }"
                "  const text = (document.body && document.body.innerText) ? document.body.innerText : '';"
                "  return { ok: false, url: location.href, title: document.title || '', text: String(text || '').slice(0, maxChars) };"
                "})()";

            auto ev = g_browserService->eval(opened.session_id, opened.target_id, js, true);
            if (!ev.success) {
                return MakeTextContent(std::string("Error: ") + ev.error, true);
            }

            nlohmann::json payload{
                {"success", true},
                {"session_id", opened.session_id},
                {"target_id", opened.target_id},
                {"result", ev.value}
            };
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_fetch_text");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_devtools_url", {
        "browser_devtools_url",
        "Get DevTools (F12) URL for a tab. Open this URL to inspect Console/Network/etc.",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"session_id", {{"type", "string"}}},
                {"target_id", {{"type", "string"}}}
            }},
            {"required", {"session_id", "target_id"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            std::string targetId = args.value("target_id", "");
            if (sessionId.empty() || targetId.empty()) {
                return MakeTextContent("Error: session_id and target_id are required", true);
            }
            std::string err;
            std::string url = g_browserService->getDevToolsUrl(sessionId, targetId, &err);
            if (url.empty()) {
                return MakeTextContent(std::string("Error: ") + (err.empty() ? "failed to resolve devtools url" : err), true);
            }
            nlohmann::json payload{{"success", true}, {"url", url}};
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_devtools_url");
            return MakeTextContent(payload.dump(), false);
        }
    });

    registry.registerTool("browser_open_devtools", {
        "browser_open_devtools",
        "Open DevTools (F12) window for a tab",
        clawdesk::RiskLevel::Medium,
        false,
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"session_id", {{"type", "string"}}},
                {"target_id", {{"type", "string"}}}
            }},
            {"required", {"session_id", "target_id"}}
        },
        [](const nlohmann::json& args) {
            if (!g_browserService) {
                return MakeTextContent("Error: BrowserService not initialized", true);
            }
            std::string sessionId = args.value("session_id", "");
            std::string targetId = args.value("target_id", "");
            if (sessionId.empty() || targetId.empty()) {
                return MakeTextContent("Error: session_id and target_id are required", true);
            }
            std::string err;
            std::string url = g_browserService->getDevToolsUrl(sessionId, targetId, &err);
            if (url.empty()) {
                return MakeTextContent(std::string("Error: ") + (err.empty() ? "failed to resolve devtools url" : err), true);
            }
            std::wstring wUrl = ToWide(url);
            if (wUrl.empty()) {
                return MakeTextContent("Error: invalid devtools url", true);
            }
            HINSTANCE h = ShellExecuteW(NULL, L"open", wUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h <= 32) {
                return MakeTextContent("Error: failed to open devtools url", true);
            }
            nlohmann::json payload{{"success", true}, {"url", url}};
            if (g_policyGuard) g_policyGuard->incrementUsageCount("browser_open_devtools");
            return MakeTextContent(payload.dump(), false);
        }
    });
}
// MCP 协议：初始化
std::string HandleMCPInitialize(const std::string& body) {
    std::string response = "{"
        "\"protocolVersion\":\"2024-11-05\","
        "\"capabilities\":{\"tools\":{}},"
        "\"serverInfo\":{"
            "\"name\":\"ClawDesk MCP Server\","
            "\"version\":\"" CLAWDESK_VERSION "\""
        "}"
    "}";
    return response;
}

// MCP 协议：列出工具
std::string HandleMCPToolsList() {
    auto tools = ToolRegistry::getInstance().getAllTools();
    nlohmann::json response;
    response["tools"] = nlohmann::json::array();
    for (const auto& tool : tools) {
        response["tools"].push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.inputSchema}
        });
    }
    return response.dump();
}

// MCP 协议：调用工具
std::string HandleMCPToolsCall(const std::string& body) {
    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(body);
    } catch (const std::exception& e) {
        return DumpMcpResponse(MakeTextContent(std::string("Error: Invalid JSON: ") + e.what(), true));
    }

    if (!payload.contains("name")) {
        return DumpMcpResponse(MakeTextContent("Error: Missing tool name", true));
    }

    std::string toolName = payload["name"].get<std::string>();
    nlohmann::json args = payload.value("arguments", nlohmann::json::object());

    if (!ToolRegistry::getInstance().hasTool(toolName)) {
        return DumpMcpResponse(MakeTextContent("Error: Unknown tool", true));
    }

    if (g_policyGuard) {
        auto decision = g_policyGuard->evaluateToolCall(toolName, args);
        if (!decision.allowed) {
            return DumpMcpResponse(MakeTextContent("Error: " + decision.reason, true));
        }
    }

    try {
        auto tool = ToolRegistry::getInstance().getTool(toolName);
        auto response = tool.handler(args);
        return DumpMcpResponse(response);
    } catch (const std::exception& e) {
        return DumpMcpResponse(MakeTextContent(std::string("Error: ") + e.what(), true));
    }
}
