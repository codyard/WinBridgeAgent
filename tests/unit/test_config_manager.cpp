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
 * ConfigManager 单元测试
 * 
 * 测试覆盖：
 * - 默认配置生成
 * - 配置文件加载和保存
 * - Auth Token 生成和验证
 * - 配置项访问和修改
 * - 线程安全性
 * - 配置验证
 */

#include "support/config_manager.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <thread>
#include <vector>

// 测试辅助函数
void deleteTestFile(const std::string& path) {
    std::remove(path.c_str());
}

bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

// 测试 1: 默认配置生成
void test_default_config_generation() {
    std::cout << "\n[测试 1] 默认配置生成..." << std::endl;
    
    const std::string testFile = "test_config_1.json";
    deleteTestFile(testFile);
    
    ConfigManager cm(testFile);
    cm.load();
    
    // 验证文件已创建
    assert(fileExists(testFile));
    std::cout << "  ✓ 配置文件已创建" << std::endl;
    
    // 验证 Auth Token 已生成
    std::string token = cm.getAuthToken();
    assert(!token.empty());
    assert(token.length() >= 32);
    std::cout << "  ✓ Auth Token 已生成 (长度: " << token.length() << ")" << std::endl;
    
    // 验证默认端口
    assert(cm.getServerPort() == 35182);
    std::cout << "  ✓ 默认端口: 35182" << std::endl;
    
    // 验证自动端口启用
    assert(cm.isAutoPortEnabled() == true);
    std::cout << "  ✓ 自动端口: 启用" << std::endl;
    
    // 验证默认白名单
    auto dirs = cm.getAllowedDirs();
    assert(!dirs.empty());
    std::cout << "  ✓ 默认目录白名单: " << dirs.size() << " 个" << std::endl;
    
    auto apps = cm.getAllowedApps();
    assert(!apps.empty());
    std::cout << "  ✓ 默认应用白名单: " << apps.size() << " 个" << std::endl;
    
    auto cmds = cm.getAllowedCommands();
    assert(!cmds.empty());
    std::cout << "  ✓ 默认命令白名单: " << cmds.size() << " 个" << std::endl;
    
    // 验证许可证为空
    assert(cm.getLicenseKey().empty());
    std::cout << "  ✓ 默认许可证: 未设置" << std::endl;
    
    deleteTestFile(testFile);
    std::cout << "[通过] 默认配置生成测试" << std::endl;
}

// 测试 2: 配置文件加载和保存
void test_config_load_save() {
    std::cout << "\n[测试 2] 配置文件加载和保存..." << std::endl;
    
    const std::string testFile = "test_config_2.json";
    deleteTestFile(testFile);
    
    // 创建并保存配置
    ConfigManager cm1(testFile);
    cm1.load();
    std::string originalToken = cm1.getAuthToken();
    cm1.setLicenseKey("test-license-key-12345");
    cm1.save();
    
    std::cout << "  ✓ 配置已保存" << std::endl;
    
    // 重新加载配置
    ConfigManager cm2(testFile);
    cm2.load();
    
    // 验证配置一致性
    assert(cm2.getAuthToken() == originalToken);
    std::cout << "  ✓ Auth Token 一致" << std::endl;
    
    assert(cm2.getLicenseKey() == "test-license-key-12345");
    std::cout << "  ✓ License Key 一致" << std::endl;
    
    assert(cm2.getServerPort() == cm1.getServerPort());
    std::cout << "  ✓ Server Port 一致" << std::endl;
    
    deleteTestFile(testFile);
    std::cout << "[通过] 配置加载和保存测试" << std::endl;
}

// 测试 3: Auth Token 自动重新生成
void test_auth_token_regeneration() {
    std::cout << "\n[测试 3] Auth Token 自动重新生成..." << std::endl;
    
    const std::string testFile = "test_config_3.json";
    deleteTestFile(testFile);
    
    // 创建一个无效的配置文件（Token 太短）
    {
        std::ofstream file(testFile);
        file << R"({
            "auth_token": "short",
            "server_port": 35182,
            "auto_port": true,
            "allowed_dirs": [],
            "allowed_apps": {},
            "allowed_commands": [],
            "license_key": ""
        })" << std::endl;
    }
    
    std::cout << "  ✓ 创建了无效 Token 的配置文件" << std::endl;
    
    // 加载配置，应该自动重新生成 Token
    ConfigManager cm(testFile);
    cm.load();
    
    std::string newToken = cm.getAuthToken();
    assert(newToken != "short");
    assert(newToken.length() >= 32);
    std::cout << "  ✓ Token 已自动重新生成 (长度: " << newToken.length() << ")" << std::endl;
    
    deleteTestFile(testFile);
    std::cout << "[通过] Auth Token 重新生成测试" << std::endl;
}

// 测试 4: 配置项修改
void test_config_modification() {
    std::cout << "\n[测试 4] 配置项修改..." << std::endl;
    
    const std::string testFile = "test_config_4.json";
    deleteTestFile(testFile);
    
    ConfigManager cm(testFile);
    cm.load();
    
    // 修改许可证
    cm.setLicenseKey("new-license-key");
    assert(cm.getLicenseKey() == "new-license-key");
    std::cout << "  ✓ License Key 修改成功" << std::endl;
    
    // 修改端口
    cm.setActualPort(12345);
    assert(cm.getServerPort() == 12345);
    std::cout << "  ✓ Server Port 修改成功" << std::endl;
    
    // 保存并重新加载
    cm.save();
    ConfigManager cm2(testFile);
    cm2.load();
    
    assert(cm2.getLicenseKey() == "new-license-key");
    assert(cm2.getServerPort() == 12345);
    std::cout << "  ✓ 修改已持久化" << std::endl;
    
    deleteTestFile(testFile);
    std::cout << "[通过] 配置修改测试" << std::endl;
}

// 测试 5: 线程安全性
void test_thread_safety() {
    std::cout << "\n[测试 5] 线程安全性..." << std::endl;
    
    const std::string testFile = "test_config_5.json";
    deleteTestFile(testFile);
    
    ConfigManager cm(testFile);
    cm.load();
    
    const int numThreads = 10;
    const int numIterations = 100;
    std::vector<std::thread> threads;
    
    // 创建多个线程并发访问配置
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&cm, numIterations]() {
            for (int j = 0; j < numIterations; ++j) {
                // 读取操作
                std::string token = cm.getAuthToken();
                int port = cm.getServerPort();
                bool autoPort = cm.isAutoPortEnabled();
                auto dirs = cm.getAllowedDirs();
                auto apps = cm.getAllowedApps();
                auto cmds = cm.getAllowedCommands();
                std::string license = cm.getLicenseKey();
                
                // 写入操作
                cm.setLicenseKey("thread-test-" + std::to_string(j));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "  ✓ " << numThreads << " 个线程完成 " << numIterations << " 次迭代" << std::endl;
    std::cout << "  ✓ 无死锁或崩溃" << std::endl;
    
    deleteTestFile(testFile);
    std::cout << "[通过] 线程安全性测试" << std::endl;
}

// 测试 6: 配置验证
void test_config_validation() {
    std::cout << "\n[测试 6] 配置验证..." << std::endl;
    
    const std::string testFile = "test_config_6.json";
    deleteTestFile(testFile);
    
    // 测试无效端口
    {
        std::ofstream file(testFile);
        file << R"({
            "auth_token": "valid-token-with-at-least-32-characters-here-12345678",
            "server_port": 99999,
            "auto_port": true,
            "allowed_dirs": [],
            "allowed_apps": {},
            "allowed_commands": [],
            "license_key": ""
        })" << std::endl;
    }
    
    ConfigManager cm(testFile);
    try {
        cm.load();
        assert(false && "应该抛出异常");
    } catch (const std::exception& e) {
        std::cout << "  ✓ 无效端口被拒绝: " << e.what() << std::endl;
    }
    
    deleteTestFile(testFile);
    std::cout << "[通过] 配置验证测试" << std::endl;
}

// 主测试函数
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "ConfigManager 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        test_default_config_generation();
        test_config_load_save();
        test_auth_token_regeneration();
        test_config_modification();
        test_thread_safety();
        test_config_validation();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "所有测试通过! ✓" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[失败] 测试异常: " << e.what() << std::endl;
        return 1;
    }
}
