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


# ClawDesk MCP Server - Clipboard Test Script
# Test clipboard read and write operations

SERVER="http://192.168.31.3:35182"

echo "=========================================="
echo "ClawDesk MCP Server - Clipboard Test"
echo "=========================================="
echo ""

# Test 1: Read clipboard
echo "Test 1: Reading clipboard content..."
curl -s "${SERVER}/clipboard" | jq .
echo ""

# Test 2: Write to clipboard
echo "Test 2: Writing to clipboard..."
curl -s -X PUT -H "Content-Type: application/json" \
  -d '{"content":"Hello from ClawDesk MCP Server!"}' \
  "${SERVER}/clipboard" | jq .
echo ""

# Test 3: Read back to verify
echo "Test 3: Reading clipboard to verify..."
curl -s "${SERVER}/clipboard" | jq .
echo ""

# Test 4: Write multi-line content
echo "Test 4: Writing multi-line content..."
curl -s -X PUT -H "Content-Type: application/json" \
  -d '{"content":"Line 1\nLine 2\nLine 3"}' \
  "${SERVER}/clipboard" | jq .
echo ""

# Test 5: Read multi-line content
echo "Test 5: Reading multi-line content..."
curl -s "${SERVER}/clipboard" | jq .
echo ""

# Test 6: Write content with special characters
echo "Test 6: Writing content with special characters..."
curl -s -X PUT -H "Content-Type: application/json" \
  -d '{"content":"Special: \"quotes\", \ttabs, and \\backslashes\\"}' \
  "${SERVER}/clipboard" | jq .
echo ""

# Test 7: Read special characters
echo "Test 7: Reading special characters..."
curl -s "${SERVER}/clipboard" | jq .
echo ""

echo "=========================================="
echo "Clipboard tests completed!"
echo "=========================================="
