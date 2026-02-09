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


# ClawDesk MCP Server - Comprehensive Feature Test Script
# Test all implemented features

SERVER="http://192.168.31.3:35182"

echo "=========================================="
echo "ClawDesk MCP Server - Feature Test Suite"
echo "=========================================="
echo ""

# Test 1: Health Check
echo "Test 1: Health Check"
curl -s "${SERVER}/health" | jq .
echo ""

# Test 2: Server Status
echo "Test 2: Server Status"
curl -s "${SERVER}/status" | jq .
echo ""

# Test 3: API List
echo "Test 3: API Endpoint List"
curl -s "${SERVER}/" | jq '.endpoints[] | .path'
echo ""

# Test 4: Disk List
echo "Test 4: Disk List"
curl -s "${SERVER}/disks" | jq '.[] | {drive, type, filesystem}'
echo ""

# Test 5: Directory Listing
echo "Test 5: Directory Listing (C:\\)"
curl -s "${SERVER}/list?path=C:\\" | jq '.[0:3]'
echo ""

# Test 6: File Reading
echo "Test 6: File Reading (count only)"
curl -s "${SERVER}/read?path=C:\\Windows\\System32\\drivers\\etc\\hosts&count=true" | jq .
echo ""

# Test 7: File Search
echo "Test 7: File Search"
curl -s "${SERVER}/search?path=C:\\Windows\\System32\\drivers\\etc\\hosts&query=localhost&case=i" | jq '{path, match_count}'
echo ""

# Test 8: Clipboard Read
echo "Test 8: Clipboard Read"
curl -s "${SERVER}/clipboard" | jq '{length, empty}'
echo ""

# Test 9: Clipboard Write
echo "Test 9: Clipboard Write"
curl -s -X PUT -H "Content-Type: application/json" \
  -d '{"content":"ClawDesk MCP Server Test"}' \
  "${SERVER}/clipboard" | jq .
echo ""

# Test 10: Clipboard Read (verify)
echo "Test 10: Clipboard Read (verify write)"
curl -s "${SERVER}/clipboard" | jq .
echo ""

# Test 11: Screenshot (PNG)
echo "Test 11: Screenshot (PNG format)"
response=$(curl -s "${SERVER}/screenshot")
echo "$response" | jq '{success, format, width, height, data_length: (.data | length)}'
echo ""

# Test 12: Screenshot (JPEG)
echo "Test 12: Screenshot (JPEG format)"
response=$(curl -s "${SERVER}/screenshot?format=jpg")
echo "$response" | jq '{success, format, width, height, data_length: (.data | length)}'
echo ""

# Test 13: Save Screenshot
echo "Test 13: Saving screenshot to file..."
curl -s "${SERVER}/screenshot" | jq -r '.data' | base64 -d > test_screenshot.png
if [ -f test_screenshot.png ]; then
    size=$(ls -lh test_screenshot.png | awk '{print $5}')
    echo "Screenshot saved: test_screenshot.png (${size})"
    rm test_screenshot.png
else
    echo "Failed to save screenshot"
fi
echo ""

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
