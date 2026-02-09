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
#ifndef LICENSE_MANAGER_H
#define LICENSE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <optional>
#include <chrono>

// 前向声明
class ConfigManager;

/**
 * LicenseStatus - 许可证状态枚举
 */
enum class LicenseStatus {
    Free,       // 免费版
    Active,     // 付费版激活
    Expired,    // 已过期
    Invalid     // 无效许可证
};

/**
 * LicenseInfo - 许可证信息
 */
struct LicenseInfo {
    LicenseStatus status;
    std::optional<std::chrono::system_clock::time_point> expiresAt;
    std::vector<std::string> features;
};

/**
 * UsageStats - 使用统计数据
 */
struct UsageStats {
    int dailyCallCount;             // 今日总调用次数
    int dailyScreenshotCount;       // 今日截图次数
    int remainingCalls;             // 剩余调用次数
    int remainingScreenshots;       // 剩余截图次数
    LicenseStatus licenseStatus;    // 许可证状态
    std::map<std::string, int> toolCalls;  // 每个工具的调用次数
};

// Open-source edition: all methods are no-op stubs, always returns Active/unlimited.
class LicenseManager {
public:
    explicit LicenseManager(ConfigManager* configManager,
                           const std::string& usageDataPath = "usage.json");

    LicenseInfo validateLicense();
    bool activateLicense(const std::string& licenseKey);
    bool isFeatureAvailable(const std::string& feature);
    bool isQuotaExceeded(const std::string& toolName);
    void incrementUsage(const std::string& toolName);
    void resetDailyCounters();
    UsageStats getUsageStats();
    bool isExpiringSoon(int days);
    LicenseStatus getLicenseStatus();
    std::optional<std::chrono::system_clock::time_point> getExpirationTime();
    bool validateApiKeyFormat(const std::string& apiKey) const;
    std::string generateApiKey() const;

private:
    bool verifyLicenseKey(const std::string& key) const;
    LicenseInfo parseLicenseKey(const std::string& key);
    void loadUsageData();
    void saveUsageData();
    void checkAndResetIfNeeded();
    std::string getCurrentDateString();
    int getFreeQuota(const std::string& toolName);

    ConfigManager* configManager_;
    std::string usageDataPath_;
    LicenseInfo cachedLicenseInfo_;
    std::chrono::system_clock::time_point lastValidationTime_;
    mutable std::mutex usageMutex_;
};

#endif // LICENSE_MANAGER_H
