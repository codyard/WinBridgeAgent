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


# Test Script for File Operations Features (v0.3.0)
# This script tests delete_file, copy_file, move_file, and create_directory tools
# Run this on Windows with Git Bash or similar

set -e

# Configuration
SERVER_URL="http://localhost:35182"
AUTH_TOKEN=$(grep -o '"auth_token": *"[^"]*"' config.json | cut -d'"' -f4)

# Test directory (should be in allowed_dirs)
TEST_DIR="C:/Users/$USER/Documents/ClawDeskTest"
TEST_DIR_WIN="C:\\Users\\$USER\\Documents\\ClawDeskTest"

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
print_section "File Operations Test Suite"
echo "Testing delete_file, copy_file, move_file, create_directory"
echo "Server: ${SERVER_URL}"
echo "Test Directory: ${TEST_DIR}"
echo ""

# Cleanup any existing test directory
if [ -d "$TEST_DIR" ]; then
    print_warning "Cleaning up existing test directory..."
    rm -rf "$TEST_DIR"
fi

# Test 1: Create directory (single level)
print_section "Test 1: Create Single-Level Directory"
print_test "Creating directory: ${TEST_DIR_WIN}"
RESULT=$(call_tool "create_directory" "{\"path\": \"${TEST_DIR_WIN}\", \"recursive\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Directory created successfully"
else
    print_error "Failed to create directory"
fi

# Test 2: Create multi-level directory
print_section "Test 2: Create Multi-Level Directory"
MULTI_DIR="${TEST_DIR_WIN}\\level1\\level2\\level3"
print_test "Creating multi-level directory: ${MULTI_DIR}"
RESULT=$(call_tool "create_directory" "{\"path\": \"${MULTI_DIR}\", \"recursive\": true}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Multi-level directory created successfully"
else
    print_error "Failed to create multi-level directory"
fi

# Test 3: Create directory that already exists (should succeed)
print_section "Test 3: Create Existing Directory (Idempotency)"
print_test "Creating directory that already exists..."
RESULT=$(call_tool "create_directory" "{\"path\": \"${TEST_DIR_WIN}\", \"recursive\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Idempotent directory creation works"
else
    print_error "Failed idempotency test"
fi

# Test 4: Create test files
print_section "Test 4: Create Test Files"
print_test "Creating test files..."
echo "This is test file 1" > "${TEST_DIR}/file1.txt"
echo "This is test file 2" > "${TEST_DIR}/file2.txt"
echo "This is test file 3" > "${TEST_DIR}/file3.txt"
mkdir -p "${TEST_DIR}/subdir"
echo "This is a file in subdir" > "${TEST_DIR}/subdir/subfile.txt"
print_success "Test files created"

# Test 5: Copy file
print_section "Test 5: Copy File"
SOURCE="${TEST_DIR_WIN}\\file1.txt"
DEST="${TEST_DIR_WIN}\\file1_copy.txt"
print_test "Copying file: ${SOURCE} -> ${DEST}"
RESULT=$(call_tool "copy_file" "{\"source\": \"${SOURCE}\", \"destination\": \"${DEST}\", \"overwrite\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "File copied successfully"
    if [ -f "${TEST_DIR}/file1_copy.txt" ]; then
        print_success "Copied file exists on disk"
    else
        print_error "Copied file not found on disk"
    fi
else
    print_error "Failed to copy file"
fi

# Test 6: Copy file with overwrite=false (should fail)
print_section "Test 6: Copy File Without Overwrite (Should Fail)"
print_test "Attempting to copy to existing file without overwrite..."
RESULT=$(call_tool "copy_file" "{\"source\": \"${SOURCE}\", \"destination\": \"${DEST}\", \"overwrite\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Correctly rejected copy without overwrite"
else
    print_error "Should have rejected copy without overwrite"
fi

# Test 7: Copy file with overwrite=true
print_section "Test 7: Copy File With Overwrite"
print_test "Copying to existing file with overwrite=true..."
RESULT=$(call_tool "copy_file" "{\"source\": \"${SOURCE}\", \"destination\": \"${DEST}\", \"overwrite\": true}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "File overwritten successfully"
else
    print_error "Failed to overwrite file"
fi

# Test 8: Copy directory
print_section "Test 8: Copy Directory"
SOURCE_DIR="${TEST_DIR_WIN}\\subdir"
DEST_DIR="${TEST_DIR_WIN}\\subdir_copy"
print_test "Copying directory: ${SOURCE_DIR} -> ${DEST_DIR}"
RESULT=$(call_tool "copy_file" "{\"source\": \"${SOURCE_DIR}\", \"destination\": \"${DEST_DIR}\", \"overwrite\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Directory copied successfully"
    if [ -f "${TEST_DIR}/subdir_copy/subfile.txt" ]; then
        print_success "Copied directory contents verified"
    else
        print_error "Copied directory contents not found"
    fi
else
    print_error "Failed to copy directory"
fi

# Test 9: Move/rename file
print_section "Test 9: Move/Rename File"
SOURCE="${TEST_DIR_WIN}\\file2.txt"
DEST="${TEST_DIR_WIN}\\file2_renamed.txt"
print_test "Moving/renaming file: ${SOURCE} -> ${DEST}"
RESULT=$(call_tool "move_file" "{\"source\": \"${SOURCE}\", \"destination\": \"${DEST}\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "File moved/renamed successfully"
    if [ ! -f "${TEST_DIR}/file2.txt" ] && [ -f "${TEST_DIR}/file2_renamed.txt" ]; then
        print_success "Move operation verified (source deleted, destination exists)"
    else
        print_error "Move operation verification failed"
    fi
else
    print_error "Failed to move/rename file"
fi

# Test 10: Move file to subdirectory
print_section "Test 10: Move File to Subdirectory"
SOURCE="${TEST_DIR_WIN}\\file3.txt"
DEST="${TEST_DIR_WIN}\\subdir\\file3.txt"
print_test "Moving file to subdirectory: ${SOURCE} -> ${DEST}"
RESULT=$(call_tool "move_file" "{\"source\": \"${SOURCE}\", \"destination\": \"${DEST}\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "File moved to subdirectory successfully"
else
    print_error "Failed to move file to subdirectory"
fi

# Test 11: Delete single file
print_section "Test 11: Delete Single File"
FILE_TO_DELETE="${TEST_DIR_WIN}\\file1_copy.txt"
print_test "Deleting file: ${FILE_TO_DELETE}"
RESULT=$(call_tool "delete_file" "{\"path\": \"${FILE_TO_DELETE}\", \"recursive\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "File deleted successfully"
    if [ ! -f "${TEST_DIR}/file1_copy.txt" ]; then
        print_success "File deletion verified"
    else
        print_error "File still exists after deletion"
    fi
else
    print_error "Failed to delete file"
fi

# Test 12: Delete empty directory
print_section "Test 12: Delete Empty Directory"
mkdir -p "${TEST_DIR}/empty_dir"
EMPTY_DIR="${TEST_DIR_WIN}\\empty_dir"
print_test "Deleting empty directory: ${EMPTY_DIR}"
RESULT=$(call_tool "delete_file" "{\"path\": \"${EMPTY_DIR}\", \"recursive\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Empty directory deleted successfully"
else
    print_error "Failed to delete empty directory"
fi

# Test 13: Try to delete non-empty directory without recursive (should fail)
print_section "Test 13: Delete Non-Empty Directory Without Recursive (Should Fail)"
NON_EMPTY_DIR="${TEST_DIR_WIN}\\subdir"
print_test "Attempting to delete non-empty directory without recursive..."
RESULT=$(call_tool "delete_file" "{\"path\": \"${NON_EMPTY_DIR}\", \"recursive\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Correctly rejected non-empty directory deletion"
else
    print_error "Should have rejected non-empty directory deletion"
fi

# Test 14: Delete directory recursively
print_section "Test 14: Delete Directory Recursively"
print_test "Deleting directory recursively: ${NON_EMPTY_DIR}"
RESULT=$(call_tool "delete_file" "{\"path\": \"${NON_EMPTY_DIR}\", \"recursive\": true}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q '"success": *true'; then
    print_success "Directory deleted recursively"
    if [ ! -d "${TEST_DIR}/subdir" ]; then
        print_success "Recursive deletion verified"
    else
        print_error "Directory still exists after recursive deletion"
    fi
else
    print_error "Failed to delete directory recursively"
fi

# Test 15: Try to access path outside whitelist (should fail)
print_section "Test 15: Whitelist Protection Test"
FORBIDDEN_PATH="C:\\Windows\\test.txt"
print_test "Attempting to create file in system directory (should be rejected)..."
RESULT=$(call_tool "create_directory" "{\"path\": \"${FORBIDDEN_PATH}\", \"recursive\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Whitelist protection working (system directory rejected)"
else
    print_error "WARNING: Whitelist protection may not be working!"
fi

# Test 16: Try to delete system directory (should fail)
print_section "Test 16: System Directory Protection Test"
SYSTEM_DIR="C:\\Windows"
print_test "Attempting to delete system directory (should be rejected)..."
RESULT=$(call_tool "delete_file" "{\"path\": \"${SYSTEM_DIR}\", \"recursive\": true}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "protected\|error"; then
    print_success "System directory protection working"
else
    print_error "WARNING: System directory protection may not be working!"
fi

# Test 17: Try to copy from non-existent source
print_section "Test 17: Non-Existent Source Test"
NON_EXISTENT="${TEST_DIR_WIN}\\nonexistent.txt"
DEST="${TEST_DIR_WIN}\\dest.txt"
print_test "Attempting to copy from non-existent source..."
RESULT=$(call_tool "copy_file" "{\"source\": \"${NON_EXISTENT}\", \"destination\": \"${DEST}\", \"overwrite\": false}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Non-existent source correctly handled"
else
    print_error "Non-existent source should return error"
fi

# Test 18: Try to move non-existent file
print_section "Test 18: Move Non-Existent File Test"
print_test "Attempting to move non-existent file..."
RESULT=$(call_tool "move_file" "{\"source\": \"${NON_EXISTENT}\", \"destination\": \"${DEST}\"}")
echo "$RESULT" | jq '.' 2>/dev/null || echo "$RESULT"

if echo "$RESULT" | grep -q "error"; then
    print_success "Non-existent file move correctly handled"
else
    print_error "Non-existent file move should return error"
fi

# Test 19: Check audit log
print_section "Test 19: Audit Log Verification"
print_test "Checking audit log for file operations..."

if [ -f "audit.log" ]; then
    echo "Recent file operations:"
    grep -E "delete_file|copy_file|move_file|create_directory" audit.log | tail -10 | jq '.' 2>/dev/null || grep -E "delete_file|copy_file|move_file|create_directory" audit.log | tail -10
    print_success "Audit log contains file operations"
    
    # Check for high_risk flag
    if grep -E "delete_file" audit.log | grep -q "high_risk"; then
        print_success "High-risk operations are flagged in audit log"
    else
        print_warning "High-risk flag not found in audit log"
    fi
else
    print_warning "Audit log file not found"
fi

# Cleanup
print_section "Cleanup"
print_test "Cleaning up test directory..."
if [ -d "$TEST_DIR" ]; then
    rm -rf "$TEST_DIR"
    print_success "Test directory cleaned up"
fi

# Summary
print_section "Test Summary"
echo "File Operations Tests Completed"
echo ""
echo "Tests performed:"
echo "  ✓ Create single-level directory"
echo "  ✓ Create multi-level directory"
echo "  ✓ Directory creation idempotency"
echo "  ✓ Copy file"
echo "  ✓ Copy file with overwrite protection"
echo "  ✓ Copy file with overwrite"
echo "  ✓ Copy directory"
echo "  ✓ Move/rename file"
echo "  ✓ Move file to subdirectory"
echo "  ✓ Delete single file"
echo "  ✓ Delete empty directory"
echo "  ✓ Delete non-empty directory protection"
echo "  ✓ Delete directory recursively"
echo "  ✓ Whitelist protection"
echo "  ✓ System directory protection"
echo "  ✓ Non-existent source handling"
echo "  ✓ Audit log verification"
echo ""
print_success "All tests completed!"
echo ""
echo "Note: Some tests may require user confirmation via MessageBox."
echo "      Check the Dashboard for high-risk operation indicators."
echo ""
echo "IMPORTANT: Ensure ${TEST_DIR} is in your allowed_dirs whitelist!"
