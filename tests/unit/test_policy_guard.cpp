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
/**
 * PolicyGuard 单元测试
 */
#include "policy/policy_guard.h"
#include "support/config_manager.h"
#include "support/license_manager.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

static std::string getTodayDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &t);
    char buffer[16];
    sprintf(buffer, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return buffer;
}

static std::string toAbsolute(const std::string& path) {
    char buffer[MAX_PATH];
    DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, buffer, NULL);
    if (len == 0 || len >= MAX_PATH) {
        return path;
    }
    return std::string(buffer);
}

static void writeConfig(const std::string& path,
                        const std::string& allowedDir,
                        const std::vector<std::string>& allowedCommands) {
    nlohmann::json cfg;
    cfg["auth_token"] = "test-token-1234567890abcdef";
    cfg["server_port"] = 35182;
    cfg["auto_port"] = true;
    cfg["listen_address"] = "127.0.0.1";
    cfg["allowed_dirs"] = nlohmann::json::array({allowedDir});
    cfg["allowed_apps"] = nlohmann::json::object();
    cfg["allowed_commands"] = allowedCommands;
    cfg["license_key"] = "";

    std::ofstream out(path, std::ios::trunc);
    out << cfg.dump(2);
}

int main() {
    std::cout << "\n[PolicyGuard] 开始测试..." << std::endl;

    const std::string configPath = "test_policy_guard_config.json";
    const std::string usagePath = "test_policy_guard_usage.json";

    fs::create_directories("test_data");
    std::string allowedDir = toAbsolute("test_data");
    writeConfig(configPath, allowedDir, {"echo"});

    // 写入使用数据，确保日期一致
    {
        nlohmann::json usage;
        usage["date"] = getTodayDate();
        usage["tool_calls"] = nlohmann::json::object();
        usage["last_reset"] = "";
        std::ofstream out(usagePath, std::ios::trunc);
        out << usage.dump(2);
    }

    ConfigManager config(configPath);
    config.load();
    LicenseManager license(&config, usagePath);
    PolicyGuard guard(&config, &license);

    // 路径白名单
    assert(guard.isPathAllowed(allowedDir + "\\file.txt"));
    assert(!guard.isPathAllowed("C:\\Windows\\system32\\cmd.exe"));
    std::cout << "  ✓ 路径白名单检查" << std::endl;

    // 命令白名单
    assert(guard.isCommandAllowed("echo hello"));
    assert(!guard.isCommandAllowed("powershell Get-Process"));
    std::cout << "  ✓ 命令白名单检查" << std::endl;

    // 低风险工具评估（避免触发确认弹窗）
    nlohmann::json args;
    args["path"] = allowedDir + "\\file.txt";
    auto decision = guard.evaluateToolCall("read_file", args);
    assert(decision.allowed);
    std::cout << "  ✓ 低风险工具评估" << std::endl;

    fs::remove(configPath);
    fs::remove(usagePath);
    fs::remove_all("test_data");

    std::cout << "[通过] PolicyGuard 测试" << std::endl;
    return 0;
}
