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
 * FileService 单元测试
 */
#include "services/file_service.h"
#include "support/config_manager.h"
#include "support/license_manager.h"
#include "policy/policy_guard.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

static std::string toAbsolute(const std::string& path) {
    char buffer[MAX_PATH];
    DWORD len = GetFullPathNameA(path.c_str(), MAX_PATH, buffer, NULL);
    if (len == 0 || len >= MAX_PATH) {
        return path;
    }
    return std::string(buffer);
}

static void writeConfig(const std::string& path, const std::string& allowedDir) {
    nlohmann::json cfg;
    cfg["auth_token"] = "test-token-1234567890abcdef";
    cfg["server_port"] = 35182;
    cfg["auto_port"] = true;
    cfg["listen_address"] = "127.0.0.1";
    cfg["allowed_dirs"] = nlohmann::json::array({allowedDir});
    cfg["allowed_apps"] = nlohmann::json::object();
    cfg["allowed_commands"] = nlohmann::json::array();
    cfg["license_key"] = "";
    std::ofstream out(path, std::ios::trunc);
    out << cfg.dump(2);
}

int main() {
    std::cout << "\n[FileService] 开始测试..." << std::endl;

    const std::string configPath = "test_file_service_config.json";
    const std::string usagePath = "test_file_service_usage.json";

    fs::create_directories("test_files");
    std::string baseDir = toAbsolute("test_files");
    writeConfig(configPath, baseDir);

    std::ofstream("test_files/sample.txt") << "hello\nworld\nsearch term\n";

    ConfigManager config(configPath);
    config.load();
    LicenseManager license(&config, usagePath);
    PolicyGuard guard(&config, &license);
    FileService service(&config, &guard);

    std::string content = service.readTextFile(baseDir + "\\sample.txt");
    assert(content.find("hello") != std::string::npos);
    std::cout << "  ✓ readTextFile" << std::endl;

    auto matches = service.searchTextInFile(baseDir + "\\sample.txt", "search");
    assert(!matches.empty());
    std::cout << "  ✓ searchTextInFile" << std::endl;

    auto entries = service.listDirectory(baseDir);
    assert(!entries.empty());
    std::cout << "  ✓ listDirectory" << std::endl;

    FindFilesParams params;
    params.query = "sample";
    params.max = 10;
    auto files = service.findFiles(params);
    assert(!files.empty());
    std::cout << "  ✓ findFiles" << std::endl;

    fs::remove(configPath);
    fs::remove(usagePath);
    fs::remove_all("test_files");

    std::cout << "[通过] FileService 测试" << std::endl;
    return 0;
}
