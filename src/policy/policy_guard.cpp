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
// Open-source edition: all policy checks are pass-through (always allow).
#include "policy/policy_guard.h"
#include "support/config_manager.h"
#include "support/license_manager.h"

PolicyGuard::PolicyGuard(ConfigManager* configManager, LicenseManager* licenseManager)
    : configManager_(configManager), licenseManager_(licenseManager) {}

PolicyDecision PolicyGuard::evaluateToolCall(const std::string& /*toolName*/,
                                             const nlohmann::json& /*args*/) {
    return {true, false, "", clawdesk::RiskLevel::Low};
}

bool PolicyGuard::isPathAllowed(const std::string& /*path*/) { return true; }
bool PolicyGuard::isAppAllowed(const std::string& /*appName*/) { return true; }
bool PolicyGuard::isCommandAllowed(const std::string& /*command*/) { return true; }

bool PolicyGuard::requestUserConfirmation(const std::string& /*toolName*/,
                                          const nlohmann::json& /*args*/,
                                          clawdesk::RiskLevel /*riskLevel*/) {
    return true;
}

bool PolicyGuard::checkUsageQuota(const std::string& /*toolName*/) { return true; }
void PolicyGuard::incrementUsageCount(const std::string& /*toolName*/) {}

clawdesk::RiskLevel PolicyGuard::getRiskLevelForTool(const std::string& /*toolName*/) const {
    return clawdesk::RiskLevel::Low;
}

std::string PolicyGuard::normalizePath(const std::string& path) const { return path; }

bool PolicyGuard::startsWith(const std::string& value, const std::string& prefix) const {
    return value.compare(0, prefix.size(), prefix) == 0;
}

std::string PolicyGuard::extractCommandName(const std::string& command) const { return command; }
