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


# ClawDesk MCP Server - Token Authentication Test Script
# This script tests the token authentication functionality

SERVER="http://192.168.31.3:35182"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "ClawDesk MCP Server - Auth Token Test"
echo "=========================================="
echo ""

# Test 1: Request without token (should fail with 401)
echo -e "${YELLOW}Test 1: Request without Authorization header${NC}"
echo "GET $SERVER/status"
RESPONSE=$(curl -s -w "\n%{http_code}" "$SERVER/status")
HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)

if [ "$HTTP_CODE" = "401" ]; then
    echo -e "${GREEN}✓ PASS${NC} - Got 401 Unauthorized as expected"
    echo "Response: $BODY"
else
    echo -e "${RED}✗ FAIL${NC} - Expected 401, got $HTTP_CODE"
    echo "Response: $BODY"
fi
echo ""

# Test 2: Request with invalid token (should fail with 401)
echo -e "${YELLOW}Test 2: Request with invalid token${NC}"
echo "GET $SERVER/status with Authorization: Bearer invalid_token"
RESPONSE=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer invalid_token" "$SERVER/status")
HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)

if [ "$HTTP_CODE" = "401" ]; then
    echo -e "${GREEN}✓ PASS${NC} - Got 401 Unauthorized as expected"
    echo "Response: $BODY"
else
    echo -e "${RED}✗ FAIL${NC} - Expected 401, got $HTTP_CODE"
    echo "Response: $BODY"
fi
echo ""

# Test 3: Request with valid token (should succeed with 200)
echo -e "${YELLOW}Test 3: Request with valid token${NC}"
echo "Please enter the auth_token from config.json on the server:"
read -r AUTH_TOKEN

if [ -z "$AUTH_TOKEN" ]; then
    echo -e "${RED}No token provided, skipping test${NC}"
else
    echo "GET $SERVER/status with Authorization: Bearer <token>"
    RESPONSE=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $AUTH_TOKEN" "$SERVER/status")
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    BODY=$(echo "$RESPONSE" | head -n-1)
    
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}✓ PASS${NC} - Got 200 OK as expected"
        echo "Response: $BODY" | jq '.' 2>/dev/null || echo "$BODY"
    else
        echo -e "${RED}✗ FAIL${NC} - Expected 200, got $HTTP_CODE"
        echo "Response: $BODY"
    fi
fi
echo ""

# Test 4: CORS preflight (OPTIONS) should work without token
echo -e "${YELLOW}Test 4: CORS preflight (OPTIONS) without token${NC}"
echo "OPTIONS $SERVER/status"
RESPONSE=$(curl -s -w "\n%{http_code}" -X OPTIONS "$SERVER/status")
HTTP_CODE=$(echo "$RESPONSE" | tail -n1)

if [ "$HTTP_CODE" = "200" ]; then
    echo -e "${GREEN}✓ PASS${NC} - Got 200 OK for OPTIONS (CORS preflight works)"
else
    echo -e "${RED}✗ FAIL${NC} - Expected 200, got $HTTP_CODE"
fi
echo ""

# Test 5: Test with valid token on different endpoints
if [ -n "$AUTH_TOKEN" ]; then
    echo -e "${YELLOW}Test 5: Testing other endpoints with valid token${NC}"
    
    # Test /health
    echo "GET $SERVER/health"
    RESPONSE=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $AUTH_TOKEN" "$SERVER/health")
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}✓ /health${NC}"
    else
        echo -e "${RED}✗ /health (got $HTTP_CODE)${NC}"
    fi
    
    # Test /disks
    echo "GET $SERVER/disks"
    RESPONSE=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $AUTH_TOKEN" "$SERVER/disks")
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}✓ /disks${NC}"
    else
        echo -e "${RED}✗ /disks (got $HTTP_CODE)${NC}"
    fi
    
    # Test /clipboard
    echo "GET $SERVER/clipboard"
    RESPONSE=$(curl -s -w "\n%{http_code}" -H "Authorization: Bearer $AUTH_TOKEN" "$SERVER/clipboard")
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}✓ /clipboard${NC}"
    else
        echo -e "${RED}✗ /clipboard (got $HTTP_CODE)${NC}"
    fi
fi

echo ""
echo "=========================================="
echo "Test completed!"
echo "=========================================="
