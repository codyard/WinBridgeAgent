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
 * CommandService 单元测试
 */
#include "services/command_service.h"
#include "support/config_manager.h"
#include "support/license_manager.h"
#include "policy/policy_guard.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

static void writeConfig(const std::string& path) {
    nlohmann::json cfg;
    cfg["auth_token"] = "test-token-1234567890abcdef";
    cfg["server_port"] = 35182;
    cfg["auto_port"] = true;
    cfg["listen_address"] = "127.0.0.1";
    cfg["allowed_dirs"] = nlohmann::json::array();
    cfg["allowed_apps"] = nlohmann::json::object();
    cfg["allowed_commands"] = nlohmann::json::array({"echo"});
    cfg["license_key"] = "";
    std::ofstream out(path, std::ios::trunc);
    out << cfg.dump(2);
}

int main() {
    std::cout << "\n[CommandService] 开始测试..." << std::endl;

    const std::string configPath = "test_command_service_config.json";
    const std::string usagePath = "test_command_service_usage.json";

    writeConfig(configPath);
    fs::remove(usagePath);

    ConfigManager config(configPath);
    config.load();
    LicenseManager license(&config, usagePath);
    PolicyGuard guard(&config, &license);
    CommandService service(&config, &guard);

    auto result = service.executeCommand("echo hello", {});
    assert(result.exitCode == 0);
    assert(result.stdoutText.find("hello") != std::string::npos);
    std::cout << "  ✓ executeCommand" << std::endl;

    fs::remove(configPath);
    fs::remove(usagePath);

    std::cout << "[通过] CommandService 测试" << std::endl;
    return 0;
}
