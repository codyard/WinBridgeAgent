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
#include "mcp/tool_registry.h"
#include <stdexcept>

ToolRegistry& ToolRegistry::getInstance() {
    static ToolRegistry instance;
    return instance;
}

void ToolRegistry::registerTool(const std::string& name, const ToolMetadata& metadata) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    tools_[name] = metadata;
}

ToolMetadata ToolRegistry::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + name);
    }
    return it->second;
}

std::vector<ToolDefinition> ToolRegistry::getAllTools() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    std::vector<ToolDefinition> defs;
    for (const auto& kv : tools_) {
        defs.push_back({kv.second.name, kv.second.description, kv.second.inputSchema});
    }
    return defs;
}

bool ToolRegistry::hasTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    return tools_.find(name) != tools_.end();
}
