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
 * LicenseManager 单元测试
 */
#include "support/license_manager.h"
#include "support/config_manager.h"
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
    cfg["allowed_commands"] = nlohmann::json::array();
    cfg["license_key"] = "";
    std::ofstream out(path, std::ios::trunc);
    out << cfg.dump(2);
}

int main() {
    std::cout << "\n[LicenseManager] 开始测试..." << std::endl;

    const std::string configPath = "test_license_config.json";
    const std::string usagePath = "test_license_usage.json";

    writeConfig(configPath);
    fs::remove(usagePath);

    ConfigManager config(configPath);
    config.load();

    LicenseManager manager(&config, usagePath);

    auto info = manager.validateLicense();
    assert(info.status == LicenseStatus::Free);
    std::cout << "  ✓ 默认免费版状态" << std::endl;

    // 模拟调用达到上限
    for (int i = 0; i < 100; ++i) {
        manager.incrementUsage("read_file");
    }
    assert(manager.isQuotaExceeded("read_file"));
    std::cout << "  ✓ 总调用次数配额限制" << std::endl;

    manager.resetDailyCounters();
    for (int i = 0; i < 20; ++i) {
        manager.incrementUsage("take_screenshot");
    }
    assert(manager.isQuotaExceeded("take_screenshot"));
    std::cout << "  ✓ 截图配额限制" << std::endl;

    fs::remove(configPath);
    fs::remove(usagePath);

    std::cout << "[通过] LicenseManager 测试" << std::endl;
    return 0;
}
