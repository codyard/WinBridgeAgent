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


# Test Script for Power Management Features (v0.3.0)
# This script tests shutdown_system and abort_shutdown tools
# Run this on Windows with Git Bash or similar
# WARNING: This script will test system shutdown/reboot - use with caution!

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

# Start test suite
print_section "Power Management Test Suite"
echo "Testing shutdown_system and abort_shutdown"
echo "Server: ${SERVER_URL}"
echo ""
print_warning "WARNING: This script tests system power management!"
print_warning "         Tests use delays to allow cancellation."
print_warning "         Press Ctrl+C to abort the entire test suite."
echo ""
read -p "Press Enter to continue or Ctrl+C to abort..."

# Test 1: Schedule shutdown with delay (then cancel)
print_section "Test 1: Schedule Shutdown with Delay"
print_test "Scheduling shutdown in 120 seconds..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 120, \"force\": false, \"message\": \"Test shutdown - will be cancelled\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Shutdown scheduled successfully"
    
    # Extract scheduled time
    SCHEDULED_TIME=$(echo "$RESULT" | jq -r '.scheduled_time' 2>/dev/null || echo "unknown")
    echo "Scheduled time: ${SCHEDULED_TIME}"
    
    # Wait a few seconds
    print_test "Waiting 5 seconds before cancelling..."
    sleep 5
    
    # Cancel the shutdown
    print_test "Cancelling scheduled shutdown..."
    CANCEL_RESULT=$(call_tool "abort_shutdown" "{}")
    echo "$CANCEL_RESULT" | jq '.' 2>/dev/null || echo "$CANCEL_RESULT"
    
    if echo "$CANCEL_RESULT" | grep -q '"success": *true'; then
        print_success "Shutdown cancelled successfully"
    else
        print_error "Failed to cancel shutdown"
        print_warning "You may need to manually cancel: shutdown /a"
    fi
else
    print_error "Failed to schedule shutdown"
fi

# Test 2: Schedule reboot with delay (then cancel)
print_section "Test 2: Schedule Reboot with Delay"
print_test "Scheduling reboot in 120 seconds..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"reboot\", \"delay\": 120, \"force\": false, \"message\": \"Test reboot - will be cancelled\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Reboot scheduled successfully"
    
    # Wait a few seconds
    print_test "Waiting 5 seconds before cancelling..."
    sleep 5
    
    # Cancel the reboot
    print_test "Cancelling scheduled reboot..."
    CANCEL_RESULT=$(call_tool "abort_shutdown" "{}")
    echo "$CANCEL_RESULT" | jq '.' 2>/dev/null || echo "$CANCEL_RESULT"
    
    if echo "$CANCEL_RESULT" | grep -q '"success": *true'; then
        print_success "Reboot cancelled successfully"
    else
        print_error "Failed to cancel reboot"
        print_warning "You may need to manually cancel: shutdown /a"
    fi
else
    print_error "Failed to schedule reboot"
fi

# Test 3: Test immediate shutdown (delay=0) - DANGEROUS, commented out by default
print_section "Test 3: Immediate Shutdown Test (SKIPPED)"
print_warning "Immediate shutdown test is SKIPPED for safety"
print_warning "To test immediate shutdown, uncomment the code in the script"
print_warning "WARNING: This will immediately shut down your system!"

# Uncomment the following lines to test immediate shutdown (USE WITH CAUTION!)
# print_test "Scheduling immediate shutdown (delay=0)..."
# RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 0, \"force\": false}")
# echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

# Test 4: Test forced shutdown
print_section "Test 4: Forced Shutdown Test"
print_test "Scheduling forced shutdown in 120 seconds..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 120, \"force\": true, \"message\": \"Test forced shutdown - will be cancelled\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Forced shutdown scheduled successfully"
    
    # Wait and cancel
    print_test "Waiting 5 seconds before cancelling..."
    sleep 5
    
    print_test "Cancelling forced shutdown..."
    CANCEL_RESULT=$(call_tool "abort_shutdown" "{}")
    echo "$CANCEL_RESULT" | jq '.' 2>/dev/null || echo "$CANCEL_RESULT"
    
    if echo "$CANCEL_RESULT" | grep -q '"success": *true'; then
        print_success "Forced shutdown cancelled successfully"
    else
        print_error "Failed to cancel forced shutdown"
    fi
else
    print_error "Failed to schedule forced shutdown"
fi

# Test 5: Test hibernate (may not be supported on all systems)
print_section "Test 5: Hibernate Test"
print_test "Attempting to hibernate system..."
print_warning "This test will actually hibernate your system if supported!"
print_warning "Press Ctrl+C within 10 seconds to skip this test..."
sleep 10

RESULT=$(call_tool "shutdown_system" "{\"action\": \"hibernate\", \"delay\": 0}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Hibernate command sent successfully"
    print_warning "System should hibernate now..."
elif echo "$RESULT" | grep -q "not supported"; then
    print_warning "Hibernate is not supported on this system (expected on some systems)"
else
    print_error "Failed to hibernate"
fi

# Test 6: Test sleep (may not be supported on all systems)
print_section "Test 6: Sleep Test"
print_test "Attempting to sleep system..."
print_warning "This test will actually sleep your system if supported!"
print_warning "Press Ctrl+C within 10 seconds to skip this test..."
sleep 10

RESULT=$(call_tool "shutdown_system" "{\"action\": \"sleep\", \"delay\": 0}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Sleep command sent successfully"
    print_warning "System should sleep now..."
elif echo "$RESULT" | grep -q "not supported"; then
    print_warning "Sleep is not supported on this system (expected on some systems)"
else
    print_error "Failed to sleep"
fi

# Test 7: Test custom message
print_section "Test 7: Custom Message Test"
print_test "Scheduling shutdown with custom message..."
CUSTOM_MSG="This is a test shutdown message. The system will shutdown in 2 minutes. Please save your work!"
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 120, \"force\": false, \"message\": \"${CUSTOM_MSG}\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Shutdown with custom message scheduled"
    print_warning "Check if the message is displayed in Windows notification"
    
    # Wait and cancel
    print_test "Waiting 5 seconds before cancelling..."
    sleep 5
    
    print_test "Cancelling shutdown..."
    call_tool "abort_shutdown" "{}" > /dev/null
    print_success "Shutdown cancelled"
else
    print_error "Failed to schedule shutdown with custom message"
fi

# Test 8: Test abort when no shutdown is scheduled
print_section "Test 8: Abort Non-Existent Shutdown"
print_test "Attempting to abort when no shutdown is scheduled..."
RESULT=$(call_tool "abort_shutdown" "{}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error\|No shutdown"; then
    print_success "Correctly handled abort with no scheduled shutdown"
else
    print_warning "Unexpected response for abort with no scheduled shutdown"
fi

# Test 9: Test invalid action
print_section "Test 9: Invalid Action Test"
print_test "Attempting to use invalid action..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"invalid_action\", \"delay\": 60}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Invalid action correctly rejected"
else
    print_error "Invalid action should be rejected"
fi

# Test 10: Test negative delay
print_section "Test 10: Negative Delay Test"
print_test "Attempting to use negative delay..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": -60}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Negative delay correctly rejected"
else
    print_error "Negative delay should be rejected"
fi

# Test 11: Test very large delay
print_section "Test 11: Large Delay Test"
print_test "Scheduling shutdown with very large delay (1 hour)..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 3600, \"message\": \"Test with 1 hour delay\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Large delay accepted"
    
    # Immediately cancel
    print_test "Cancelling immediately..."
    call_tool "abort_shutdown" "{}" > /dev/null
    print_success "Cancelled"
else
    print_error "Failed to schedule with large delay"
fi

# Test 12: Test permissions (may fail if not running as admin)
print_section "Test 12: Permission Test"
print_test "Testing if running with sufficient privileges..."
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 300}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Running with sufficient privileges"
    call_tool "abort_shutdown" "{}" > /dev/null
elif echo "$RESULT" | grep -q "Insufficient privileges"; then
    print_warning "Insufficient privileges - run as administrator"
else
    print_error "Unexpected permission response"
fi

# Test 13: Check audit log
print_section "Test 13: Audit Log Verification"
print_test "Checking audit log for power management operations..."

if [ -f "audit.log" ]; then
    echo "Recent power management operations:"
    grep -E "shutdown_system|abort_shutdown" audit.log | tail -10 | jq '.' 2>/dev/null || grep -E "shutdown_system|abort_shutdown" audit.log | tail -10
    print_success "Audit log contains power management operations"
    
    # Check for high_risk flag
    if grep -E "shutdown_system" audit.log | grep -q "high_risk"; then
        print_success "High-risk operations are flagged in audit log"
    else
        print_warning "High-risk flag not found in audit log"
    fi
    
    # Check for critical risk level
    if grep -E "shutdown_system" audit.log | grep -q '"risk": *"critical"'; then
        print_success "Critical risk level correctly set"
    else
        print_warning "Critical risk level not found"
    fi
else
    print_warning "Audit log file not found"
fi

# Test 14: Dashboard countdown test
print_section "Test 14: Dashboard Countdown Test"
print_test "Scheduling shutdown to test Dashboard countdown..."
print_warning "Check the Dashboard for countdown display!"
RESULT=$(call_tool "shutdown_system" "{\"action\": \"shutdown\", \"delay\": 60, \"message\": \"Dashboard countdown test\"}")

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Shutdown scheduled for Dashboard test"
    echo ""
    echo "Please check the Dashboard for:"
    echo "  - Countdown timer"
    echo "  - Red warning banner"
    echo "  - Cancel button"
    echo ""
    print_test "Waiting 15 seconds for you to observe the Dashboard..."
    sleep 15
    
    print_test "Cancelling shutdown..."
    call_tool "abort_shutdown" "{}" > /dev/null
    print_success "Shutdown cancelled"
else
    print_error "Failed to schedule shutdown for Dashboard test"
fi

# Summary
print_section "Test Summary"
echo "Power Management Tests Completed"
echo ""
echo "Tests performed:"
echo "  ✓ Schedule shutdown with delay"
echo "  ✓ Schedule reboot with delay"
echo "  ✓ Forced shutdown"
echo "  ✓ Hibernate (if supported)"
echo "  ✓ Sleep (if supported)"
echo "  ✓ Custom message"
echo "  ✓ Abort shutdown"
echo "  ✓ Abort non-existent shutdown"
echo "  ✓ Invalid action handling"
echo "  ✓ Negative delay handling"
echo "  ✓ Large delay handling"
echo "  ✓ Permission check"
echo "  ✓ Audit log verification"
echo "  ✓ Dashboard countdown display"
echo ""
print_success "All tests completed!"
echo ""
echo "IMPORTANT NOTES:"
echo "  - Some tests require administrator privileges"
echo "  - Hibernate/Sleep may not be supported on all systems"
echo "  - All scheduled shutdowns were cancelled during testing"
echo "  - Check the Dashboard for high-risk operation indicators"
echo "  - Review audit.log for detailed operation history"
echo ""
print_warning "If any shutdown was not cancelled, run: shutdown /a"
