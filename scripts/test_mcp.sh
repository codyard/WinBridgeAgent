#!/bin/bash
# Copyright (C) 2026 Codyard
#
# This file is part of WinBridgeAgent.
#
# WinBridgeAgent is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# WinBridgeAgent is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with WinBridgeAgent. If not, see <https://www.gnu.org/licenses/\>.


# ClawDesk MCP Server - MCP Protocol Test Script
# Test Model Context Protocol endpoints

SERVER="http://192.168.31.3:35182"

echo "=========================================="
echo "ClawDesk MCP Server - MCP Protocol Test"
echo "=========================================="
echo ""

# Test 1: MCP Initialize
echo "Test 1: MCP Initialize"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test-client","version":"1.0.0"}}' \
  "${SERVER}/mcp/initialize" | jq .
echo ""

# Test 2: MCP Tools List
echo "Test 2: MCP Tools List"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{}' \
  "${SERVER}/mcp/tools/list" | jq '.tools | length as $count | "Found \($count) tools"'
echo ""
echo "Available tools:"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{}' \
  "${SERVER}/mcp/tools/list" | jq '.tools[] | {name, description}'
echo ""

# Test 3: MCP Tools Call - list_windows
echo "Test 3: MCP Tools Call - list_windows"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"name":"list_windows","arguments":{}}' \
  "${SERVER}/mcp/tools/call" | jq '{isError, content_type: .content[0].type}'
echo ""

# Test 4: MCP Tools Call - list_processes
echo "Test 4: MCP Tools Call - list_processes"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"name":"list_processes","arguments":{}}' \
  "${SERVER}/mcp/tools/call" | jq '{isError, content_type: .content[0].type}'
echo ""

# Test 5: MCP Tools Call - unknown tool
echo "Test 5: MCP Tools Call - unknown tool (should error)"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"name":"unknown_tool","arguments":{}}' \
  "${SERVER}/mcp/tools/call" | jq .
echo ""

echo "=========================================="
echo "MCP Protocol tests completed!"
echo "=========================================="
