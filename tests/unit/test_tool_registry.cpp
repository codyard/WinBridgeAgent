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
 * ToolRegistry 单元测试
 */
#include "mcp/tool_registry.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "\n[ToolRegistry] 开始测试..." << std::endl;

    auto& registry = ToolRegistry::getInstance();
    ToolMetadata meta;
    meta.name = "unit_test_tool";
    meta.description = "Unit test tool";
    meta.riskLevel = clawdesk::RiskLevel::Low;
    meta.requiresConfirmation = false;
    meta.inputSchema = nlohmann::json::object();
    meta.handler = [](const nlohmann::json&) {
        return nlohmann::json{
            {"content", nlohmann::json::array({{{"type", "text"}, {"text", "ok"}}})},
            {"isError", false}
        };
    };

    registry.registerTool(meta.name, meta);
    assert(registry.hasTool("unit_test_tool"));
    auto loaded = registry.getTool("unit_test_tool");
    assert(loaded.name == "unit_test_tool");

    auto tools = registry.getAllTools();
    bool found = false;
    for (const auto& tool : tools) {
        if (tool.name == "unit_test_tool") {
            found = true;
            break;
        }
    }
    assert(found);
    std::cout << "  ✓ 注册与查询" << std::endl;

    std::cout << "[通过] ToolRegistry 测试" << std::endl;
    return 0;
}
