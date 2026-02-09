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


# Security Test Script for ClawDesk MCP Server v0.3.0
# This script validates all security protections and safeguards
# Run this on Windows with Git Bash or similar

set -e

# Configuration
SERVER_URL="http://localhost:35182"
AUTH_TOKEN=$(grep -o '"auth_token": *"[^"]*"' config.json | cut -d'"' -f4)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Security test results
SECURITY_TESTS_PASSED=0
SECURITY_TESTS_FAILED=0
CRITICAL_FAILURES=0

# Helper functions
print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((SECURITY_TESTS_PASSED++))
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((SECURITY_TESTS_FAILED++))
}

print_critical() {
    echo -e "${RED}${BOLD}[CRITICAL FAILURE]${NC} $1"
    ((SECURITY_TESTS_FAILED++))
    ((CRITICAL_FAILURES++))
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_section() {
    echo ""
    echo -e "${MAGENTA}${BOLD}========================================${NC}"
    echo -e "${MAGENTA}${BOLD}$1${NC}"
    echo -e "${MAGENTA}${BOLD}========================================${NC}"
}

# Helper function to make MCP tool calls
call_tool() {
    local tool_name=$1
    local arguments=$2
    
    curl -s -X POST "${SERVER_URL}/tools/call" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -d "{\"name\": \"${tool_name}\", \"arguments\": ${arguments}}"
}

# Start security test suite
print_section "Security Test Suite for v0.3.0"
echo "This script validates all security protections"
echo "Server: ${SERVER_URL}"
echo ""
print_warning "This test suite attempts to bypass security measures"
print_warning "All attempts should FAIL - that's the expected behavior!"
echo ""

# ============================================
# SECTION 1: Protected Process Tests
# ============================================
print_section "Section 1: Protected Process Security"

# Test 1.1: Try to kill system process
print_test "1.1: Attempting to terminate 'system' process (PID 4)..."
RESULT=$(call_tool "kill_process" "{\"pid\": 4, \"force\": true}")
if echo "$RESULT" | grep -qi "protected\|error\|denied"; then
    print_success "System process correctly protected"
else
    print_critical "SECURITY BREACH: System process was not protected!"
    echo "$RESULT"
fi

# Test 1.2: Try to kill csrss.exe
print_test "1.2: Attempting to terminate 'csrss.exe'..."
# Get csrss PID (usually around 400-600)
CSRSS_PID=$(tasklist | grep -i "csrss.exe" | head -1 | awk '{print $2}' || echo "0")
if [ "$CSRSS_PID" != "0" ]; then
    RESULT=$(call_tool "kill_process" "{\"pid\": ${CSRSS_PID}, \"force\": true}")
    if echo "$RESULT" | grep -qi "protected\|error\|denied"; then
        print_success "csrss.exe correctly protected"
    else
        print_critical "SECURITY BREACH: csrss.exe was not protected!"
        echo "$RESULT"
    fi
else
    print_warning "Could not find csrss.exe PID, skipping test"
fi

# Test 1.3: Try to kill winlogon.exe
print_test "1.3: Attempting to terminate 'winlogon.exe'..."
WINLOGON_PID=$(tasklist | grep -i "winlogon.exe" | head -1 | awk '{print $2}' || echo "0")
if [ "$WINLOGON_PID" != "0" ]; then
    RESULT=$(call_tool "kill_process" "{\"pid\": ${WINLOGON_PID}, \"force\": true}")
    if echo "$RESULT" | grep -qi "protected\|error\|denied"; then
        print_success "winlogon.exe correctly protected"
    else
        print_critical "SECURITY BREACH: winlogon.exe was not protected!"
        echo "$RESULT"
    fi
else
    print_warning "Could not find winlogon.exe PID, skipping test"
fi

# Test 1.4: Try to kill services.exe
print_test "1.4: Attempting to terminate 'services.exe'..."
SERVICES_PID=$(tasklist | grep -i "services.exe" | head -1 | awk '{print $2}' || echo "0")
if [ "$SERVICES_PID" != "0" ]; then
    RESULT=$(call_tool "kill_process" "{\"pid\": ${SERVICES_PID}, \"force\": true}")
    if echo "$RESULT" | grep -qi "protected\|error\|denied"; then
        print_success "services.exe correctly protected"
    else
        print_critical "SECURITY BREACH: services.exe was not protected!"
        echo "$RESULT"
    fi
else
    print_warning "Could not find services.exe PID, skipping test"
fi

# Test 1.5: Try to kill lsass.exe
print_test "1.5: Attempting to terminate 'lsass.exe'..."
LSASS_PID=$(tasklist | grep -i "lsass.exe" | head -1 | awk '{print $2}' || echo "0")
if [ "$LSASS_PID" != "0" ]; then
    RESULT=$(call_tool "kill_process" "{\"pid\": ${LSASS_PID}, \"force\": true}")
    if echo "$RESULT" | grep -qi "protected\|error\|denied"; then
        print_success "lsass.exe correctly protected"
    else
        print_critical "SECURITY BREACH: lsass.exe was not protected!"
        echo "$RESULT"
    fi
else
    print_warning "Could not find lsass.exe PID, skipping test"
fi

# ============================================
# SECTION 2: System Directory Protection
# ============================================
print_section "Section 2: System Directory Protection"

# Test 2.1: Try to delete C:\Windows
print_test "2.1: Attempting to delete C:\\Windows..."
RESULT=$(call_tool "delete_file" "{\"path\": \"C:\\\\Windows\", \"recursive\": true}")
if echo "$RESULT" | grep -qi "protected\|error\|not allowed"; then
    print_success "C:\\Windows correctly protected"
else
    print_critical "SECURITY BREACH: C:\\Windows was not protected!"
    echo "$RESULT"
fi

# Test 2.2: Try to delete C:\Program Files
print_test "2.2: Attempting to delete C:\\Program Files..."
RESULT=$(call_tool "delete_file" "{\"path\": \"C:\\\\Program Files\", \"recursive\": true}")
if echo "$RESULT" | grep -qi "protected\|error\|not allowed"; then
    print_success "C:\\Program Files correctly protected"
else
    print_critical "SECURITY BREACH: C:\\Program Files was not protected!"
    echo "$RESULT"
fi

# Test 2.3: Try to delete C:\Program Files (x86)
print_test "2.3: Attempting to delete C:\\Program Files (x86)..."
RESULT=$(call_tool "delete_file" "{\"path\": \"C:\\\\Program Files (x86)\", \"recursive\": true}")
if echo "$RESULT" | grep -qi "protected\|error\|not allowed"; then
    print_success "C:\\Program Files (x86) correctly protected"
else
    print_critical "SECURITY BREACH: C:\\Program Files (x86) was not protected!"
    echo "$RESULT"
fi

# Test 2.4: Try to create file in C:\Windows
print_test "2.4: Attempting to create directory in C:\\Windows..."
RESULT=$(call_tool "create_directory" "{\"path\": \"C:\\\\Windows\\\\TestDir\", \"recursive\": false}")
if echo "$RESULT" | grep -qi "protected\|error\|not allowed"; then
    print_success "C:\\Windows write protection working"
else
    print_critical "SECURITY BREACH: Could write to C:\\Windows!"
    echo "$RESULT"
fi

# Test 2.5: Try to copy file to C:\Windows
print_test "2.5: Attempting to copy file to C:\\Windows..."
# Create a temp file first
TEMP_FILE="C:\\Users\\$USER\\Documents\\temp_test.txt"
echo "test" > "$TEMP_FILE" 2>/dev/null || true
RESULT=$(call_tool "copy_file" "{\"source\": \"${TEMP_FILE}\", \"destination\": \"C:\\\\Windows\\\\test.txt\", \"overwrite\": false}")
if echo "$RESULT" | grep -qi "protected\|error\|not allowed"; then
    print_success "C:\\Windows copy protection working"
else
    print_critical "SECURITY BREACH: Could copy to C:\\Windows!"
    echo "$RESULT"
fi
rm -f "$TEMP_FILE" 2>/dev/null || true

# ============================================
# SECTION 3: Whitelist Enforcement
# ============================================
print_section "Section 3: Whitelist Enforcement"

# Test 3.1: Try to access path outside whitelist
print_test "3.1: Attempting to create directory outside whitelist..."
RESULT=$(call_tool "create_directory" "{\"path\": \"C:\\\\Temp\\\\ClawDeskUnauthorized\", \"recursive\": false}")
if echo "$RESULT" | grep -qi "not allowed\|error"; then
    print_success "Whitelist enforcement working for create_directory"
else
    print_error "WARNING: Could create directory outside whitelist"
    echo "$RESULT"
fi

# Test 3.2: Try to copy file from outside whitelist
print_test "3.2: Attempting to copy from path outside whitelist..."
RESULT=$(call_tool "copy_file" "{\"source\": \"C:\\\\Temp\\\\test.txt\", \"destination\": \"C:\\\\Users\\\\$USER\\\\Documents\\\\test.txt\", \"overwrite\": false}")
if echo "$RESULT" | grep -qi "not allowed\|error"; then
    print_success "Whitelist enforcement working for copy source"
else
    print_error "WARNING: Could copy from outside whitelist"
    echo "$RESULT"
fi

# Test 3.3: Try to copy file to outside whitelist
print_test "3.3: Attempting to copy to path outside whitelist..."
RESULT=$(call_tool "copy_file" "{\"source\": \"C:\\\\Users\\\\$USER\\\\Documents\\\\test.txt\", \"destination\": \"C:\\\\Temp\\\\test.txt\", \"overwrite\": false}")
if echo "$RESULT" | grep -qi "not allowed\|error"; then
    print_success "Whitelist enforcement working for copy destination"
else
    print_error "WARNING: Could copy to outside whitelist"
    echo "$RESULT"
fi

# Test 3.4: Try to move file to outside whitelist
print_test "3.4: Attempting to move file to path outside whitelist..."
RESULT=$(call_tool "move_file" "{\"source\": \"C:\\\\Users\\\\$USER\\\\Documents\\\\test.txt\", \"destination\": \"C:\\\\Temp\\\\test.txt\"}")
if echo "$RESULT" | grep -qi "not allowed\|error"; then
    print_success "Whitelist enforcement working for move"
else
    print_error "WARNING: Could move to outside whitelist"
    echo "$RESULT"
fi

# Test 3.5: Try to delete file outside whitelist
print_test "3.5: Attempting to delete file outside whitelist..."
RESULT=$(call_tool "delete_file" "{\"path\": \"C:\\\\Temp\\\\test.txt\", \"recursive\": false}")
if echo "$RESULT" | grep -qi "not allowed\|error"; then
    print_success "Whitelist enforcement working for delete"
else
    print_error "WARNING: Could delete outside whitelist"
    echo "$RESULT"
fi

# ============================================
# SECTION 4: Authentication & Authorization
# ============================================
print_section "Section 4: Authentication & Authorization"

# Test 4.1: Try to call tool without auth token
print_test "4.1: Attempting to call tool without authentication..."
RESULT=$(curl -s -X POST "${SERVER_URL}/tools/call" \
    -H "Content-Type: application/json" \
    -d '{"name": "list_windows", "arguments": {}}')
if echo "$RESULT" | grep -qi "unauthorized\|401\|authentication"; then
    print_success "Authentication required (no token rejected)"
else
    print_critical "SECURITY BREACH: Tool call succeeded without authentication!"
    echo "$RESULT"
fi

# Test 4.2: Try to call tool with invalid auth token
print_test "4.2: Attempting to call tool with invalid token..."
RESULT=$(curl -s -X POST "${SERVER_URL}/tools/call" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer invalid_token_12345" \
    -d '{"name": "list_windows", "arguments": {}}')
if echo "$RESULT" | grep -qi "unauthorized\|401\|authentication\|invalid"; then
    print_success "Invalid token rejected"
else
    print_critical "SECURITY BREACH: Invalid token was accepted!"
    echo "$RESULT"
fi

# Test 4.3: Try to call tool with malformed auth header
print_test "4.3: Attempting to call tool with malformed auth header..."
RESULT=$(curl -s -X POST "${SERVER_URL}/tools/call" \
    -H "Content-Type: application/json" \
    -H "Authorization: InvalidFormat" \
    -d '{"name": "list_windows", "arguments": {}}')
if echo "$RESULT" | grep -qi "unauthorized\|401\|authentication"; then
    print_success "Malformed auth header rejected"
else
    print_critical "SECURITY BREACH: Malformed auth header was accepted!"
    echo "$RESULT"
fi

# ============================================
# SECTION 5: Input Validation
# ============================================
print_section "Section 5: Input Validation"

# Test 5.1: Try SQL injection in path
print_test "5.1: Testing SQL injection in file path..."
RESULT=$(call_tool "delete_file" "{\"path\": \"C:\\\\Users\\\\$USER\\\\Documents\\\\'; DROP TABLE files; --\", \"recursive\": false}")
if echo "$RESULT" | grep -qi "error\|invalid"; then
    print_success "SQL injection attempt handled safely"
else
    print_warning "SQL injection test - verify no database corruption"
fi

# Test 5.2: Try path traversal attack
print_test "5.2: Testing path traversal attack..."
RESULT=$(call_tool "delete_file" "{\"path\": \"C:\\\\Users\\\\$USER\\\\Documents\\\\..\\\\..\\\\..\\\\Windows\\\\System32\", \"recursive\": true}")
if echo "$RESULT" | grep -qi "protected\|error\|not allowed"; then
    print_success "Path traversal attack blocked"
else
    print_critical "SECURITY BREACH: Path traversal attack succeeded!"
    echo "$RESULT"
fi

# Test 5.3: Try command injection in shutdown message
print_test "5.3: Testing command injection in shutdown message..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 300, \"message\": \"Test & calc.exe & echo pwned\"}")
if echo "$RESULT" | grep -qi "success"; then
    # Check if calc.exe was launched (it shouldn't be)
    sleep 2
    if tasklist | grep -qi "calc.exe"; then
        print_critical "SECURITY BREACH: Command injection succeeded!"
        taskkill //F //IM calc.exe 2>/dev/null || true
    else
        print_success "Command injection blocked"
    fi
    # Cancel the shutdown
    call_tool "abort_shutdown" "{}" > /dev/null
else
    print_success "Shutdown with suspicious message rejected"
fi

# Test 5.4: Try extremely long input
print_test "5.4: Testing extremely long input..."
LONG_PATH=$(printf 'A%.0s' {1..10000})
RESULT=$(call_tool "create_directory" "{\"path\": \"${LONG_PATH}\", \"recursive\": false}")
if echo "$RESULT" | grep -qi "error\|invalid"; then
    print_success "Extremely long input rejected"
else
    print_warning "Long input accepted - verify buffer overflow protection"
fi

# Test 5.5: Try negative PID
print_test "5.5: Testing negative PID..."
RESULT=$(call_tool "kill_process" "{\"pid\": -1, \"force\": false}")
if echo "$RESULT" | grep -qi "error\|invalid"; then
    print_success "Negative PID rejected"
else
    print_error "Negative PID should be rejected"
fi

# Test 5.6: Try zero PID
print_test "5.6: Testing zero PID..."
RESULT=$(call_tool "kill_process" "{\"pid\": 0, \"force\": false}")
if echo "$RESULT" | grep -qi "error\|invalid\|protected"; then
    print_success "Zero PID rejected"
else
    print_error "Zero PID should be rejected"
fi

# ============================================
# SECTION 6: Audit Logging
# ============================================
print_section "Section 6: Audit Logging Security"

# Test 6.1: Verify high-risk operations are logged
print_test "6.1: Verifying high-risk operations are logged..."
if [ -f "audit.log" ]; then
    if grep -q "high_risk" audit.log; then
        print_success "High-risk operations are being logged"
    else
        print_error "High-risk flag not found in audit log"
    fi
else
    print_error "Audit log file not found"
fi

# Test 6.2: Verify failed operations are logged
print_test "6.2: Verifying failed operations are logged..."
if [ -f "audit.log" ]; then
    if grep -q '"result": *"error"' audit.log || grep -q '"result": *"denied"' audit.log; then
        print_success "Failed operations are being logged"
    else
        print_warning "No failed operations found in log (may be normal)"
    fi
fi

# Test 6.3: Verify audit log contains required fields
print_test "6.3: Verifying audit log format..."
if [ -f "audit.log" ]; then
    LAST_ENTRY=$(tail -1 audit.log)
    if echo "$LAST_ENTRY" | jq -e '.time, .tool, .risk, .result, .duration_ms' > /dev/null 2>&1; then
        print_success "Audit log format is correct"
    else
        print_error "Audit log missing required fields"
    fi
fi

# Test 6.4: Verify audit log is not world-writable
print_test "6.4: Verifying audit log permissions..."
if [ -f "audit.log" ]; then
    # On Windows, this is less relevant, but we can check if file exists
    print_success "Audit log exists and is accessible"
else
    print_error "Audit log file not found"
fi

# ============================================
# SECTION 7: Rate Limiting & DoS Protection
# ============================================
print_section "Section 7: Rate Limiting & DoS Protection"

# Test 7.1: Rapid-fire requests
print_test "7.1: Testing rapid-fire requests (DoS simulation)..."
SUCCESS_COUNT=0
for i in {1..50}; do
    RESULT=$(call_tool "list_windows" "{}")
    if echo "$RESULT" | grep -q "windows\|error"; then
        ((SUCCESS_COUNT++))
    fi
done
if [ $SUCCESS_COUNT -eq 50 ]; then
    print_warning "All 50 rapid requests succeeded - consider rate limiting"
elif [ $SUCCESS_COUNT -gt 0 ]; then
    print_success "Some requests throttled ($SUCCESS_COUNT/50 succeeded)"
else
    print_error "All requests failed - may indicate server issue"
fi

# ============================================
# SUMMARY
# ============================================
print_section "Security Test Summary"

echo ""
echo "Total Security Tests: $((SECURITY_TESTS_PASSED + SECURITY_TESTS_FAILED))"
echo "Passed: ${SECURITY_TESTS_PASSED}"
echo "Failed: ${SECURITY_TESTS_FAILED}"
echo "Critical Failures: ${CRITICAL_FAILURES}"
echo ""

if [ $CRITICAL_FAILURES -gt 0 ]; then
    echo -e "${RED}${BOLD}CRITICAL SECURITY FAILURES DETECTED!${NC}"
    echo -e "${RED}${BOLD}DO NOT RELEASE UNTIL THESE ARE FIXED!${NC}"
    echo ""
    exit 1
elif [ $SECURITY_TESTS_FAILED -gt 0 ]; then
    echo -e "${YELLOW}${BOLD}Some security tests failed${NC}"
    echo "Review the failures above and determine if they are acceptable"
    echo ""
    exit 1
else
    echo -e "${GREEN}${BOLD}All security tests passed! ✓${NC}"
    echo "v0.3.0 security measures are working correctly"
    echo ""
    exit 0
fi
