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


# Master Test Runner for ClawDesk MCP Server v0.3.0
# Executes all test suites and generates a summary report

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Log file
LOG_FILE="test_results_$(date +%Y%m%d_%H%M%S).log"

# Helper functions
print_header() {
    echo ""
    echo -e "${CYAN}${BOLD}========================================${NC}"
    echo -e "${CYAN}${BOLD}$1${NC}"
    echo -e "${CYAN}${BOLD}========================================${NC}"
    echo ""
}

print_section() {
    echo ""
    echo -e "${BLUE}${BOLD}$1${NC}"
    echo -e "${BLUE}----------------------------------------${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${CYAN}ℹ${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    print_section "Checking Prerequisites"
    
    local all_ok=true
    
    # Check if server is running
    if curl -s http://localhost:35182/tools/list > /dev/null 2>&1; then
        print_success "Server is running"
    else
        print_error "Server is not running or not accessible"
        print_info "Please start WinBridgeAgent.exe before running tests"
        all_ok=false
    fi
    
    # Check if config.json exists
    if [ -f "config.json" ]; then
        print_success "config.json found"
    else
        print_error "config.json not found"
        print_info "Please create config.json from config.template.json"
        all_ok=false
    fi
    
    # Check if jq is installed
    if command -v jq &> /dev/null; then
        print_success "jq is installed"
    else
        print_warning "jq is not installed (optional, but recommended)"
        print_info "Install with: choco install jq"
    fi
    
    # Check if test scripts exist
    if [ -f "test_process_management.sh" ]; then
        print_success "test_process_management.sh found"
    else
        print_error "test_process_management.sh not found"
        all_ok=false
    fi
    
    if [ -f "test_file_operations.sh" ]; then
        print_success "test_file_operations.sh found"
    else
        print_error "test_file_operations.sh not found"
        all_ok=false
    fi
    
    if [ -f "test_power_management.sh" ]; then
        print_success "test_power_management.sh found"
    else
        print_error "test_power_management.sh not found"
        all_ok=false
    fi
    
    if [ "$all_ok" = false ]; then
        echo ""
        print_error "Prerequisites check failed. Please fix the issues above."
        exit 1
    fi
    
    echo ""
    print_success "All prerequisites met"
}

# Run a test suite
run_test_suite() {
    local test_name=$1
    local test_script=$2
    
    print_header "Running: $test_name"
    
    echo "Start time: $(date)" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    
    if bash "$test_script" 2>&1 | tee -a "$LOG_FILE"; then
        print_success "$test_name completed successfully"
        ((PASSED_TESTS++))
    else
        print_error "$test_name failed"
        ((FAILED_TESTS++))
    fi
    
    echo "" | tee -a "$LOG_FILE"
    echo "End time: $(date)" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    
    ((TOTAL_TESTS++))
}

# Generate summary report
generate_summary() {
    print_header "Test Summary Report"
    
    echo "Test Execution Date: $(date)" | tee -a "$LOG_FILE"
    echo "Log File: $LOG_FILE" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    
    print_section "Test Suites Executed"
    echo "Total Test Suites: $TOTAL_TESTS" | tee -a "$LOG_FILE"
    echo "Passed: $PASSED_TESTS" | tee -a "$LOG_FILE"
    echo "Failed: $FAILED_TESTS" | tee -a "$LOG_FILE"
    echo "Skipped: $SKIPPED_TESTS" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    
    # Calculate success rate
    if [ $TOTAL_TESTS -gt 0 ]; then
        SUCCESS_RATE=$((PASSED_TESTS * 100 / TOTAL_TESTS))
        echo "Success Rate: ${SUCCESS_RATE}%" | tee -a "$LOG_FILE"
    fi
    
    echo "" | tee -a "$LOG_FILE"
    
    # Check audit log
    print_section "Audit Log Analysis"
    if [ -f "audit.log" ]; then
        local total_ops=$(wc -l < audit.log)
        local high_risk_ops=$(grep -c "high_risk" audit.log || echo "0")
        local errors=$(grep -c '"result": *"error"' audit.log || echo "0")
        
        echo "Total operations logged: $total_ops" | tee -a "$LOG_FILE"
        echo "High-risk operations: $high_risk_ops" | tee -a "$LOG_FILE"
        echo "Operations with errors: $errors" | tee -a "$LOG_FILE"
        
        # Show recent high-risk operations
        echo "" | tee -a "$LOG_FILE"
        echo "Recent high-risk operations:" | tee -a "$LOG_FILE"
        grep "high_risk" audit.log | tail -5 | tee -a "$LOG_FILE"
    else
        print_warning "audit.log not found"
    fi
    
    echo "" | tee -a "$LOG_FILE"
    
    # Final verdict
    print_section "Final Verdict"
    if [ $FAILED_TESTS -eq 0 ]; then
        print_success "All test suites passed! ✓"
        echo "" | tee -a "$LOG_FILE"
        echo -e "${GREEN}${BOLD}v0.3.0 is ready for release!${NC}" | tee -a "$LOG_FILE"
    else
        print_error "Some test suites failed! ✗"
        echo "" | tee -a "$LOG_FILE"
        echo -e "${RED}${BOLD}Please fix the failures before release!${NC}" | tee -a "$LOG_FILE"
    fi
    
    echo "" | tee -a "$LOG_FILE"
    print_info "Full test log saved to: $LOG_FILE"
}

# Main execution
main() {
    print_header "ClawDesk MCP Server v0.3.0 Test Suite"
    
    echo "This script will run all v0.3.0 test suites:"
    echo "  1. Process Management Tests"
    echo "  2. File Operations Tests"
    echo "  3. Power Management Tests"
    echo ""
    print_warning "WARNING: Power management tests will schedule shutdowns (then cancel them)"
    print_warning "         Make sure you have saved all your work!"
    echo ""
    read -p "Press Enter to continue or Ctrl+C to abort..."
    
    # Check prerequisites
    check_prerequisites
    
    # Run test suites
    run_test_suite "Process Management Tests" "test_process_management.sh"
    run_test_suite "File Operations Tests" "test_file_operations.sh"
    
    # Ask before running power management tests
    echo ""
    print_warning "About to run Power Management Tests"
    print_warning "These tests will schedule system shutdowns (then cancel them)"
    read -p "Continue with power management tests? (y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        run_test_suite "Power Management Tests" "test_power_management.sh"
    else
        print_warning "Power Management Tests skipped by user"
        ((SKIPPED_TESTS++))
        ((TOTAL_TESTS++))
    fi
    
    # Generate summary
    generate_summary
    
    # Exit with appropriate code
    if [ $FAILED_TESTS -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

# Run main function
main "$@"
