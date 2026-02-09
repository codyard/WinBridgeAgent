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


# ClawDesk MCP Server - Windows and Processes Test Script
# Test window list, process list, and command execution

SERVER="http://192.168.31.3:35182"

echo "=========================================="
echo "ClawDesk MCP Server - Windows & Processes Test"
echo "=========================================="
echo ""

# Test 1: Get window list
echo "Test 1: Getting window list..."
curl -s "${SERVER}/windows" | jq '. | length as $count | "Found \($count) windows"'
echo ""
echo "Sample windows:"
curl -s "${SERVER}/windows" | jq '.[0:3] | .[] | {title, class, visible, minimized}'
echo ""

# Test 2: Get process list
echo "Test 2: Getting process list..."
curl -s "${SERVER}/processes" | jq '. | length as $count | "Found \($count) processes"'
echo ""
echo "Sample processes:"
curl -s "${SERVER}/processes" | jq '.[0:5] | .[] | {pid, name, memory_kb}'
echo ""

# Test 3: Execute simple command
echo "Test 3: Executing command (echo)"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"command":"echo Hello from ClawDesk"}' \
  "${SERVER}/execute" | jq .
echo ""

# Test 4: Execute dir command
echo "Test 4: Executing command (dir)"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"command":"dir C:\\ /b"}' \
  "${SERVER}/execute" | jq '{success, exit_code, output: (.output | split("\\n") | .[0:5])}'
echo ""

# Test 5: Execute systeminfo
echo "Test 5: Executing command (systeminfo - first 10 lines)"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"command":"systeminfo"}' \
  "${SERVER}/execute" | jq '{success, exit_code, output: (.output | split("\\n") | .[0:10])}'
echo ""

# Test 6: Execute ipconfig
echo "Test 6: Executing command (ipconfig)"
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"command":"ipconfig"}' \
  "${SERVER}/execute" | jq '{success, exit_code}'
echo ""

# Test 7: Find specific window
echo "Test 7: Finding Chrome windows..."
curl -s "${SERVER}/windows" | jq '[.[] | select(.title | contains("Chrome"))] | length as $count | "Found \($count) Chrome windows"'
echo ""

# Test 8: Find specific process
echo "Test 8: Finding explorer.exe process..."
curl -s "${SERVER}/processes" | jq '[.[] | select(.name == "explorer.exe")] | .[] | {pid, name, memory_kb}'
echo ""

echo "=========================================="
echo "Windows & Processes tests completed!"
echo "=========================================="
