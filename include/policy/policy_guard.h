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
#ifndef CLAWDESK_POLICY_GUARD_H
#define CLAWDESK_POLICY_GUARD_H

#include <string>
#include <nlohmann/json.hpp>
#include "support/audit_logger.h"

class ConfigManager;
class LicenseManager;

struct PolicyDecision {
    bool allowed;
    bool requiresConfirmation;
    std::string reason;
    clawdesk::RiskLevel riskLevel;
};

// Open-source edition: all checks are pass-through (always allow).
class PolicyGuard {
public:
    PolicyGuard(ConfigManager* configManager, LicenseManager* licenseManager);

    PolicyDecision evaluateToolCall(const std::string& toolName,
                                    const nlohmann::json& args);

    bool isPathAllowed(const std::string& path);
    bool isAppAllowed(const std::string& appName);
    bool isCommandAllowed(const std::string& command);

    bool requestUserConfirmation(const std::string& toolName,
                                 const nlohmann::json& args,
                                 clawdesk::RiskLevel riskLevel);

    bool checkUsageQuota(const std::string& toolName);
    void incrementUsageCount(const std::string& toolName);

private:
    clawdesk::RiskLevel getRiskLevelForTool(const std::string& toolName) const;
    std::string normalizePath(const std::string& path) const;
    bool startsWith(const std::string& value, const std::string& prefix) const;
    std::string extractCommandName(const std::string& command) const;

    ConfigManager* configManager_;
    LicenseManager* licenseManager_;
};

#endif // CLAWDESK_POLICY_GUARD_H
