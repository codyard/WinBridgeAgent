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


# ClawDesk MCP Server - Screenshot Test Script
# Test screenshot capture functionality

SERVER="http://192.168.31.3:35182"

echo "=========================================="
echo "ClawDesk MCP Server - Screenshot Test"
echo "=========================================="
echo ""

# Test 1: Capture screenshot in PNG format
echo "Test 1: Capturing screenshot (PNG format)..."
response=$(curl -s "${SERVER}/screenshot")
echo "$response" | jq -r '.success, .format, .width, .height, (.data | length)'
echo ""

# Test 2: Capture screenshot in JPEG format
echo "Test 2: Capturing screenshot (JPEG format)..."
response=$(curl -s "${SERVER}/screenshot?format=jpg")
echo "$response" | jq -r '.success, .format, .width, .height, (.data | length)'
echo ""

# Test 3: Save screenshot to file (PNG)
echo "Test 3: Saving PNG screenshot to file..."
curl -s "${SERVER}/screenshot" | jq -r '.data' | base64 -d > screenshot_test.png
if [ -f screenshot_test.png ]; then
    size=$(ls -lh screenshot_test.png | awk '{print $5}')
    echo "Screenshot saved: screenshot_test.png (${size})"
else
    echo "Failed to save screenshot"
fi
echo ""

# Test 4: Save screenshot to file (JPEG)
echo "Test 4: Saving JPEG screenshot to file..."
curl -s "${SERVER}/screenshot?format=jpg" | jq -r '.data' | base64 -d > screenshot_test.jpg
if [ -f screenshot_test.jpg ]; then
    size=$(ls -lh screenshot_test.jpg | awk '{print $5}')
    echo "Screenshot saved: screenshot_test.jpg (${size})"
else
    echo "Failed to save screenshot"
fi
echo ""

echo "=========================================="
echo "Screenshot tests completed!"
echo "Check screenshot_test.png and screenshot_test.jpg"
echo "=========================================="
