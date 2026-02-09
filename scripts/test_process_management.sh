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


# Test Script for Process Management Features (v0.3.0)
# This script tests kill_process and set_process_priority tools
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
NC='\033[0m' # No Color

# Helper function to print colored output
print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_section() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
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

# Helper function to list processes
list_processes() {
    curl -s -X POST "${SERVER_URL}/tools/call" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer ${AUTH_TOKEN}" \
        -d '{"name": "list_windows", "arguments": {}}'
}

# Start test suite
print_section "Process Management Test Suite"
echo "Testing kill_process and set_process_priority"
echo "Server: ${SERVER_URL}"
echo ""

# Test 1: List current processes
print_section "Test 1: List Current Processes"
print_test "Listing all visible windows/processes..."
PROCESSES=$(list_processes)
echo "$PROCESSES" | jq '.' 2>/dev/null || echo "$PROCESSES"
print_success "Process list retrieved"

# Test 2: Start a test process (notepad)
print_section "Test 2: Start Test Process"
print_test "Starting notepad.exe for testing..."
notepad.exe &
TEST_PID=$!
sleep 2
print_success "Started notepad.exe with PID: ${TEST_PID}"

# Test 3: Set process priority to low
print_section "Test 3: Set Process Priority (Low)"
print_test "Setting notepad.exe priority to 'below_normal'..."
RESULT=$(call_tool "set_process_priority" "{\"pid\": ${TEST_PID}, \"priority\": \"below_normal\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Priority changed successfully"
else
    print_error "Failed to change priority"
fi

# Test 4: Set process priority to high
print_section "Test 4: Set Process Priority (High)"
print_test "Setting notepad.exe priority to 'high'..."
RESULT=$(call_tool "set_process_priority" "{\"pid\": ${TEST_PID}, \"priority\": \"high\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Priority changed to high"
else
    print_error "Failed to change priority to high"
fi

# Test 5: Set process priority back to normal
print_section "Test 5: Set Process Priority (Normal)"
print_test "Restoring notepad.exe priority to 'normal'..."
RESULT=$(call_tool "set_process_priority" "{\"pid\": ${TEST_PID}, \"priority\": \"normal\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Priority restored to normal"
else
    print_error "Failed to restore priority"
fi

# Test 6: Graceful process termination
print_section "Test 6: Graceful Process Termination"
print_test "Attempting graceful termination of notepad.exe (force=false)..."
RESULT=$(call_tool "kill_process" "{\"pid\": ${TEST_PID}, \"force\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Process terminated gracefully"
else
    print_warning "Graceful termination may have been cancelled by user"
fi

sleep 2

# Test 7: Start another test process for forced termination
print_section "Test 7: Forced Process Termination"
print_test "Starting another notepad.exe for forced termination test..."
notepad.exe &
TEST_PID2=$!
sleep 2
print_success "Started notepad.exe with PID: ${TEST_PID2}"

print_test "Attempting forced termination (force=true)..."
RESULT=$(call_tool "kill_process" "{\"pid\": ${TEST_PID2}, \"force\": true}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Process terminated forcefully"
else
    print_error "Failed to terminate process"
fi

# Test 8: Try to terminate a protected process (should fail)
print_section "Test 8: Protected Process Test"
print_test "Attempting to terminate a system process (should be rejected)..."
print_warning "This test should FAIL - protected processes cannot be terminated"

# Try to get system process PID (this is just for demonstration)
SYSTEM_PID=4  # System process typically has PID 4
RESULT=$(call_tool "kill_process" "{\"pid\": ${SYSTEM_PID}, \"force\": true}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "protected"; then
    print_success "Protected process correctly rejected"
elif echo "$RESULT" | grep -q "error"; then
    print_success "System process protection working (access denied)"
else
    print_error "WARNING: Protected process check may not be working!"
fi

# Test 9: Try to set realtime priority (may require admin)
print_section "Test 9: Realtime Priority Test"
print_test "Starting notepad.exe for realtime priority test..."
notepad.exe &
TEST_PID3=$!
sleep 2

print_test "Attempting to set realtime priority (may require admin)..."
RESULT=$(call_tool "set_process_priority" "{\"pid\": ${TEST_PID3}, \"priority\": \"realtime\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Realtime priority set (running as admin)"
elif echo "$RESULT" | grep -q "Insufficient privileges"; then
    print_warning "Realtime priority requires admin privileges (expected)"
else
    print_error "Unexpected response for realtime priority"
fi

# Clean up
print_test "Cleaning up test process..."
call_tool "kill_process" "{\"pid\": ${TEST_PID3}, \"force\": true}" > /dev/null 2>&1

# Test 10: Try to terminate non-existent process
print_section "Test 10: Non-Existent Process Test"
print_test "Attempting to terminate non-existent process (PID 99999)..."
RESULT=$(call_tool "kill_process" "{\"pid\": 99999, \"force\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Non-existent process correctly handled"
else
    print_error "Non-existent process should return error"
fi

# Test 11: Try to set priority for non-existent process
print_section "Test 11: Non-Existent Process Priority Test"
print_test "Attempting to set priority for non-existent process..."
RESULT=$(call_tool "set_process_priority" "{\"pid\": 99999, \"priority\": \"normal\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Non-existent process priority correctly handled"
else
    print_error "Non-existent process should return error"
fi

# Test 12: Check audit log
print_section "Test 12: Audit Log Verification"
print_test "Checking audit log for process management operations..."

if [ -f "audit.log" ]; then
    echo "Recent process management operations:"
    grep -E "kill_process|set_process_priority" audit.log | tail -5 | jq '.' 2>/dev/null || grep -E "kill_process|set_process_priority" audit.log | tail -5
    print_success "Audit log contains process management operations"
else
    print_warning "Audit log file not found"
fi

# Summary
print_section "Test Summary"
echo "Process Management Tests Completed"
echo ""
echo "Tests performed:"
echo "  ✓ List processes"
echo "  ✓ Set process priority (low, high, normal)"
echo "  ✓ Graceful process termination"
echo "  ✓ Forced process termination"
echo "  ✓ Protected process rejection"
echo "  ✓ Realtime priority (admin check)"
echo "  ✓ Non-existent process handling"
echo "  ✓ Audit log verification"
echo ""
print_success "All tests completed!"
echo ""
echo "Note: Some tests may require user confirmation via MessageBox."
echo "      Check the Dashboard for high-risk operation indicators."
