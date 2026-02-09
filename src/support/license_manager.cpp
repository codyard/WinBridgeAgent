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
// Open-source edition: license always active, no quota limits.
#include "support/license_manager.h"
#include "support/config_manager.h"
#include <random>
#include <climits>
#include <iomanip>
#include <sstream>

LicenseManager::LicenseManager(ConfigManager* configManager, const std::string& usageDataPath)
    : configManager_(configManager),
      usageDataPath_(usageDataPath),
      cachedLicenseInfo_{LicenseStatus::Active, std::nullopt, {}},
      lastValidationTime_(std::chrono::system_clock::now()) {
}

LicenseInfo LicenseManager::validateLicense() {
    return {LicenseStatus::Active, std::nullopt, {}};
}

bool LicenseManager::activateLicense(const std::string&) { return true; }
bool LicenseManager::isFeatureAvailable(const std::string&) { return true; }
bool LicenseManager::isQuotaExceeded(const std::string&) { return false; }
void LicenseManager::incrementUsage(const std::string&) {}
void LicenseManager::resetDailyCounters() {}

UsageStats LicenseManager::getUsageStats() {
    return {0, 0, INT_MAX, INT_MAX, LicenseStatus::Active, {}};
}

bool LicenseManager::isExpiringSoon(int) { return false; }
LicenseStatus LicenseManager::getLicenseStatus() { return LicenseStatus::Active; }

std::optional<std::chrono::system_clock::time_point> LicenseManager::getExpirationTime() {
    return std::nullopt;
}

bool LicenseManager::validateApiKeyFormat(const std::string&) const { return true; }

std::string LicenseManager::generateApiKey() const {
    static const char kHex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::string key = "ak-";
    for (int i = 0; i < 32; ++i) key.push_back(kHex[dis(gen)]);
    return key;
}

bool LicenseManager::verifyLicenseKey(const std::string&) const { return true; }

LicenseInfo LicenseManager::parseLicenseKey(const std::string&) {
    return {LicenseStatus::Active, std::nullopt, {}};
}

void LicenseManager::loadUsageData() {}
void LicenseManager::saveUsageData() {}
void LicenseManager::checkAndResetIfNeeded() {}

std::string LicenseManager::getCurrentDateString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

int LicenseManager::getFreeQuota(const std::string&) { return INT_MAX; }
