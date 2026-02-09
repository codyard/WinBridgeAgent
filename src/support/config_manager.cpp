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
#include "support/config_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>
#include <windows.h>

using json = nlohmann::json;

// ===== 构造函数 =====

ConfigManager::ConfigManager(const std::string& configPath)
    : configPath_(configPath) {
}

// ===== 公共方法 =====

// Open-source edition: security fields (auth_token, license_key, allowed_*, security section) removed.
void ConfigManager::load() {
    std::lock_guard<std::mutex> lock(configMutex_);

    std::ifstream file(configPath_);

    if (!file.is_open()) {
        std::cout << "配置文件不存在，生成默认配置: " << configPath_ << std::endl;
        config_ = generateDefaultConfig();

        std::ofstream outFile(configPath_);
        if (!outFile.is_open()) {
            throw std::runtime_error("无法创建配置文件: " + configPath_);
        }

        json j;
        j["server_port"] = config_.server_port;
        j["auto_port"] = config_.auto_port;
        j["listen_address"] = config_.listen_address;
        j["language"] = config_.language;
        j["auto_startup"] = config_.auto_startup;
        j["daemon_enabled"] = config_.daemon_enabled;
        j["command_timeout_seconds"] = config_.command_timeout_seconds;
        j["server"] = {
            {"port", config_.server_port},
            {"auto_port", config_.auto_port},
            {"listen_address", config_.listen_address}
        };
        j["appearance"] = {
            {"dashboard_auto_show", config_.dashboard_auto_show},
            {"dashboard_always_on_top", config_.dashboard_always_on_top},
            {"tray_icon_style", config_.tray_icon_style},
            {"log_retention_days", config_.log_retention_days}
        };
        j["update"] = {
            {"enabled", config_.auto_update_enabled},
            {"check_interval_hours", config_.update_check_interval_hours},
            {"channel", config_.update_channel},
            {"github_repo", config_.github_repo},
            {"verify_signature", config_.update_verify_signature},
            {"last_check", config_.last_update_check},
            {"skipped_version", config_.skipped_version}
        };

        outFile << j.dump(4) << std::endl;
        outFile.close();

        std::cout << "默认配置已保存到: " << configPath_ << std::endl;
        return;
    }

    try {
        json j;
        file >> j;
        file.close();

        config_.server_port = j.value("server_port", 35182);
        config_.auto_port = j.value("auto_port", true);
        config_.listen_address = j.value("listen_address", "0.0.0.0");
        config_.language = j.value("language", "en");
        config_.auto_startup = j.value("auto_startup", false);
        config_.api_key = j.value("api_key", "");
        config_.dashboard_auto_show = j.value("dashboard_auto_show", true);
        config_.dashboard_always_on_top = j.value("dashboard_always_on_top", false);
        config_.tray_icon_style = j.value("tray_icon_style", "normal");
        config_.log_retention_days = j.value("log_retention_days", 30);
        config_.daemon_enabled = j.value("daemon_enabled", true);
        config_.command_timeout_seconds = j.value("command_timeout_seconds", 30);

        config_.auto_update_enabled = j.value("auto_update_enabled", true);
        config_.update_check_interval_hours = j.value("update_check_interval_hours", 6);
        config_.update_channel = j.value("update_channel", "stable");
        config_.github_repo = j.value("github_repo", "codyard/WinBridgeAgent");
        config_.update_verify_signature = j.value("update_verify_signature", true);
        config_.last_update_check = j.value("last_update_check", "");
        config_.skipped_version = j.value("skipped_version", "");

        if (j.contains("update") && j["update"].is_object()) {
            const auto& upd = j["update"];
            config_.auto_update_enabled = upd.value("enabled", config_.auto_update_enabled);
            config_.update_check_interval_hours = upd.value("check_interval_hours", config_.update_check_interval_hours);
            config_.update_channel = upd.value("channel", config_.update_channel);
            config_.github_repo = upd.value("github_repo", config_.github_repo);
            config_.update_verify_signature = upd.value("verify_signature", config_.update_verify_signature);
            config_.last_update_check = upd.value("last_check", config_.last_update_check);
            config_.skipped_version = upd.value("skipped_version", config_.skipped_version);
        }

        if (j.contains("server") && j["server"].is_object()) {
            const auto& server = j["server"];
            config_.server_port = server.value("port", config_.server_port);
            config_.auto_port = server.value("auto_port", config_.auto_port);
            config_.listen_address = server.value("listen_address", config_.listen_address);
        }

        if (j.contains("appearance") && j["appearance"].is_object()) {
            const auto& appearance = j["appearance"];
            config_.dashboard_auto_show = appearance.value("dashboard_auto_show", config_.dashboard_auto_show);
            config_.dashboard_always_on_top = appearance.value("dashboard_always_on_top", config_.dashboard_always_on_top);
            config_.tray_icon_style = appearance.value("tray_icon_style", config_.tray_icon_style);
            config_.log_retention_days = appearance.value("log_retention_days", config_.log_retention_days);
        }

        if (!validateConfig(config_)) {
            throw std::runtime_error("配置文件验证失败");
        }

        std::cout << "配置加载成功: " << configPath_ << std::endl;

    } catch (const json::exception& e) {
        throw std::runtime_error("配置文件格式错误: " + std::string(e.what()));
    }
}

void ConfigManager::save() {
    std::lock_guard<std::mutex> lock(configMutex_);

    if (!validateConfig(config_)) {
        throw std::runtime_error("配置验证失败");
    }

    std::string tempPath = configPath_ + ".tmp";
    std::ofstream file(tempPath, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开配置文件进行保存: " + tempPath);
    }

    json j;
    j["server_port"] = config_.server_port;
    j["auto_port"] = config_.auto_port;
    j["listen_address"] = config_.listen_address;
    j["language"] = config_.language;
    j["auto_startup"] = config_.auto_startup;
    j["api_key"] = config_.api_key;
    j["daemon_enabled"] = config_.daemon_enabled;
    j["command_timeout_seconds"] = config_.command_timeout_seconds;
    j["server"] = {
        {"port", config_.server_port},
        {"auto_port", config_.auto_port},
        {"listen_address", config_.listen_address}
    };
    j["appearance"] = {
        {"dashboard_auto_show", config_.dashboard_auto_show},
        {"dashboard_always_on_top", config_.dashboard_always_on_top},
        {"tray_icon_style", config_.tray_icon_style},
        {"log_retention_days", config_.log_retention_days}
    };
    j["update"] = {
        {"enabled", config_.auto_update_enabled},
        {"check_interval_hours", config_.update_check_interval_hours},
        {"channel", config_.update_channel},
        {"github_repo", config_.github_repo},
        {"verify_signature", config_.update_verify_signature},
        {"last_check", config_.last_update_check},
        {"skipped_version", config_.skipped_version}
    };

    file << j.dump(4) << std::endl;
    file.close();

    if (!MoveFileExA(tempPath.c_str(), configPath_.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        std::remove(tempPath.c_str());
        throw std::runtime_error("无法替换配置文件: " + configPath_);
    }

    std::cout << "配置已保存到: " << configPath_ << std::endl;
}

// ===== 配置项访问器 =====

std::string ConfigManager::getAuthToken() const {
    return "";
}

std::vector<std::string> ConfigManager::getAllowedDirs() const {
    return {};
}

std::map<std::string, std::string> ConfigManager::getAllowedApps() const {
    return {};
}

std::vector<std::string> ConfigManager::getAllowedCommands() const {
    return {};
}

std::string ConfigManager::getLicenseKey() const {
    return "";
}

int ConfigManager::getServerPort() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.server_port;
}

bool ConfigManager::isAutoPortEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.auto_port;
}

std::string ConfigManager::getListenAddress() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.listen_address;
}

std::string ConfigManager::getLanguage() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.language;
}

bool ConfigManager::isAutoStartupEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.auto_startup;
}

std::string ConfigManager::getApiKey() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.api_key;
}

bool ConfigManager::isHighRiskConfirmationEnabled() const {
    return false;
}

bool ConfigManager::isDashboardAutoShowEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.dashboard_auto_show;
}

bool ConfigManager::isDashboardAlwaysOnTopEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.dashboard_always_on_top;
}

std::string ConfigManager::getTrayIconStyle() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.tray_icon_style;
}

int ConfigManager::getLogRetentionDays() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.log_retention_days;
}

bool ConfigManager::isDaemonEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.daemon_enabled;
}

int ConfigManager::getCommandTimeoutSeconds() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.command_timeout_seconds > 0 ? config_.command_timeout_seconds : 30;
}
// ===== 配置项修改器 =====

void ConfigManager::setLicenseKey(const std::string&) {}

void ConfigManager::setActualPort(int port) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.server_port = port;
}

void ConfigManager::setAutoPort(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.auto_port = enabled;
}

void ConfigManager::setListenAddress(const std::string& address) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.listen_address = address;
}

void ConfigManager::setAuthToken(const std::string&) {}

void ConfigManager::setAllowedDirs(const std::vector<std::string>&) {}

void ConfigManager::setAllowedApps(const std::map<std::string, std::string>&) {}

void ConfigManager::setAllowedCommands(const std::vector<std::string>&) {}

void ConfigManager::setLanguage(const std::string& language) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.language = language;
}

void ConfigManager::setAutoStartup(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.auto_startup = enabled;
}

void ConfigManager::setApiKey(const std::string& apiKey) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.api_key = apiKey;
}

void ConfigManager::setHighRiskConfirmations(bool) {}

void ConfigManager::setDashboardAutoShow(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.dashboard_auto_show = enabled;
}

void ConfigManager::setDashboardAlwaysOnTop(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.dashboard_always_on_top = enabled;
}

void ConfigManager::setTrayIconStyle(const std::string& style) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.tray_icon_style = style;
}

void ConfigManager::setLogRetentionDays(int days) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.log_retention_days = days;
}

void ConfigManager::setDaemonEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.daemon_enabled = enabled;
}
// ===== 更新配置访问器 =====

bool ConfigManager::isAutoUpdateEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.auto_update_enabled;
}

int ConfigManager::getUpdateCheckIntervalHours() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.update_check_interval_hours;
}

std::string ConfigManager::getUpdateChannel() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.update_channel;
}

std::string ConfigManager::getGitHubRepo() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.github_repo;
}

bool ConfigManager::isUpdateVerifySignatureEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.update_verify_signature;
}

std::string ConfigManager::getLastUpdateCheck() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.last_update_check;
}

std::string ConfigManager::getSkippedVersion() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.skipped_version;
}

// ===== 更新配置修改器 =====

void ConfigManager::setAutoUpdateEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.auto_update_enabled = enabled;
}

void ConfigManager::setUpdateCheckIntervalHours(int hours) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.update_check_interval_hours = hours;
}

void ConfigManager::setUpdateChannel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.update_channel = channel;
}

void ConfigManager::setGitHubRepo(const std::string& repo) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.github_repo = repo;
}

void ConfigManager::setUpdateVerifySignature(bool enabled) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.update_verify_signature = enabled;
}

void ConfigManager::setLastUpdateCheck(const std::string& timestamp) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.last_update_check = timestamp;
}

void ConfigManager::setSkippedVersion(const std::string& version) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.skipped_version = version;
}

// ===== 私有方法 =====

ServerConfig ConfigManager::generateDefaultConfig() {
    ServerConfig config;
    config.server_port = 35182;
    config.auto_port = true;
    config.listen_address = "0.0.0.0";
    config.language = "en";
    config.auto_startup = false;
    config.api_key = "";
    config.dashboard_auto_show = true;
    config.dashboard_always_on_top = false;
    config.tray_icon_style = "normal";
    config.log_retention_days = 30;
    config.daemon_enabled = true;
    config.command_timeout_seconds = 30;
    config.auto_update_enabled = true;
    config.update_check_interval_hours = 6;
    config.update_channel = "stable";
    config.github_repo = "codyard/WinBridgeAgent";
    config.update_verify_signature = true;
    config.last_update_check = "";
    config.skipped_version = "";
    return config;
}

std::string ConfigManager::generateAuthToken() {
    return "";
}

bool ConfigManager::validateConfig(const ServerConfig& config) const {
    if (config.server_port < 0 || config.server_port > 65535) {
        std::cerr << "配置验证失败: server_port 超出有效范围 (0-65535)" << std::endl;
        return false;
    }
    if (!config.auto_port && config.server_port != 0 &&
        (config.server_port < 1024 || config.server_port > 65535)) {
        std::cerr << "配置验证失败: server_port 超出有效范围 (1024-65535)" << std::endl;
        return false;
    }
    if (config.log_retention_days <= 0) {
        std::cerr << "配置验证失败: log_retention_days 必须为正整数" << std::endl;
        return false;
    }
    return true;
}
