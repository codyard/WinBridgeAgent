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
#ifndef CLAWDESK_TOOL_REGISTRY_H
#define CLAWDESK_TOOL_REGISTRY_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include "support/audit_logger.h"

struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json inputSchema;
};

struct ToolMetadata {
    std::string name;
    std::string description;
    clawdesk::RiskLevel riskLevel;
    bool requiresConfirmation;
    nlohmann::json inputSchema;
    std::function<nlohmann::json(const nlohmann::json&)> handler;
};

class ToolRegistry {
public:
    static ToolRegistry& getInstance();

    void registerTool(const std::string& name, const ToolMetadata& metadata);
    ToolMetadata getTool(const std::string& name) const;
    std::vector<ToolDefinition> getAllTools() const;
    bool hasTool(const std::string& name) const;

private:
    ToolRegistry() = default;
    std::map<std::string, ToolMetadata> tools_;
    mutable std::mutex registryMutex_;
};

#endif // CLAWDESK_TOOL_REGISTRY_H
