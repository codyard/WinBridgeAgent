# ClawDesk MCP Server v0.3.0 Test Guide

## Overview

This guide provides instructions for executing comprehensive tests for v0.3.0 features. The test scripts validate process management, file operations, and power management functionality.

## Prerequisites

### System Requirements

- **Operating System**: Windows 10/11 (x64, x86, or ARM64)
- **Privileges**: Administrator rights recommended for full test coverage
- **Tools**: Git Bash or similar bash shell for Windows

### Server Setup

1. **Build the Server** (if not already built):

    ```bash
    ../scripts/build.sh
    ```

2. **Configure the Server**:
    - Copy `resources/config.template.json` to `config.json`
    - Update `allowed_dirs` to include test directories:
        ```json
        {
            "allowed_dirs": [
                "C:/Users/YourUsername/Documents",
                "C:/Users/YourUsername/Downloads",
                "C:/Users/YourUsername/Desktop"
            ]
        }
        ```

3. **Start the Server**:

    ```bash
    cd build/x64  # or build/x86, build-arm64
    ./ClawDeskMCP.exe
    ```

4. **Verify Server is Running**:
    - Check system tray for ClawDesk icon
    - Verify port in `runtime.json` (default: 35182)

### Test Environment Setup

1. **Install jq** (JSON processor):

    ```bash
    # Using Chocolatey
    choco install jq

    # Or download from https://stedolan.github.io/jq/
    ```

2. **Verify Auth Token**:

    ```bash
    grep "auth_token" config.json
    ```

3. **Test Server Connectivity**:
    ```bash
    curl http://localhost:35182/tools/list \
      -H "Authorization: Bearer YOUR_AUTH_TOKEN"
    ```

## Test Execution

### Test Suite 1: Process Management

**Script**: `../scripts/test_process_management.sh`

**What it tests**:

- Process priority adjustment (idle, below_normal, normal, above_normal, high, realtime)
- Graceful process termination (force=false)
- Forced process termination (force=true)
- Protected process rejection (system, csrss, winlogon, etc.)
- Non-existent process handling
- Audit log recording

**Run the test**:

```bash
../scripts/test_process_management.sh
```

**Expected behavior**:

- Notepad windows will open and close automatically
- User confirmation dialogs may appear (high-risk operations)
- All tests should pass except realtime priority (requires admin)

**Admin-only tests**:

- Realtime priority setting requires administrator privileges
- Run Git Bash as Administrator to test this feature

**Safety notes**:

- Tests use notepad.exe (safe, non-critical process)
- Protected process tests should fail (by design)
- All test processes are cleaned up automatically

### Test Suite 2: File Operations

**Script**: `../scripts/test_file_operations.sh`

**What it tests**:

- Directory creation (single-level and multi-level)
- File copying (with and without overwrite)
- Directory copying (recursive)
- File moving/renaming
- File and directory deletion (single and recursive)
- Whitelist protection
- System directory protection
- Non-existent file handling
- Audit log recording

**Run the test**:

```bash
../scripts/test_file_operations.sh
```

**Expected behavior**:

- Creates test directory: `C:/Users/YourUsername/Documents/ClawDeskTest`
- Creates, copies, moves, and deletes test files
- User confirmation dialogs may appear (high-risk operations)
- All tests should pass if directory is in whitelist

**Important**:

- Ensure `C:/Users/YourUsername/Documents` is in `allowed_dirs`
- System directory tests should fail (by design)
- Test directory is cleaned up automatically

**Safety notes**:

- All operations are confined to test directory
- System directories are protected
- No important files are affected

### Test Suite 3: Power Management

**Script**: `../scripts/test_power_management.sh`

**What it tests**:

- Shutdown scheduling with delay
- Reboot scheduling with delay
- Hibernate functionality (if supported)
- Sleep functionality (if supported)
- Forced shutdown
- Custom shutdown messages
- Shutdown cancellation (abort_shutdown)
- Invalid action handling
- Permission checks
- Dashboard countdown display
- Audit log recording

**⚠️ WARNING**: This test suite interacts with system power management!

**Run the test**:

```bash
../scripts/test_power_management.sh
```

**Expected behavior**:

- Shutdown/reboot operations are scheduled with delays
- All scheduled operations are automatically cancelled
- Windows shutdown notifications may appear
- Dashboard should show countdown timer
- User confirmation dialogs will appear (critical-risk operations)

**Safety features**:

- All tests use delays (60-120 seconds)
- All operations are cancelled automatically
- Immediate shutdown test is commented out by default
- Hibernate/Sleep tests have 10-second abort window

**Admin requirements**:

- Power management requires SE_SHUTDOWN_NAME privilege
- Run as Administrator for full test coverage

**Manual abort**:
If a shutdown is not cancelled automatically:

```bash
shutdown /a
```

## Test Results Verification

### 1. Check Console Output

Each test script provides colored output:

- **BLUE [TEST]**: Test being executed
- **GREEN [PASS]**: Test passed
- **RED [FAIL]**: Test failed
- **YELLOW [WARN]**: Warning or expected failure

### 2. Check Audit Log

```bash
# View recent operations
tail -50 audit.log | jq '.'

# Filter by tool
grep "kill_process" audit.log | jq '.'
grep "delete_file" audit.log | jq '.'
grep "shutdown_system" audit.log | jq '.'

# Check high-risk operations
grep "high_risk" audit.log | jq '.'
```

**Expected audit log entries**:

```json
{
    "time": "2026-02-04T12:00:00.000Z",
    "tool": "kill_process",
    "risk": "high",
    "result": "ok",
    "duration_ms": 15,
    "high_risk": true,
    "details": {
        "pid": 1234,
        "process_name": "notepad.exe",
        "forced": false
    }
}
```

### 3. Check Dashboard

Open the Dashboard and verify:

- **High-risk operations** are marked in red
- **Shutdown countdown** appears when scheduled
- **Cancel button** is available during countdown
- **Operation statistics** are updated
- **High-risk counter** increments

### 4. Verify File System

For file operations test:

```bash
# Test directory should be cleaned up
ls "C:/Users/$USER/Documents/ClawDeskTest"  # Should not exist
```

### 5. Check System Tray

- Server should still be running
- No error notifications
- Status should show "Running"

## Troubleshooting

### Test Script Fails to Start

**Problem**: `auth_token not found`

**Solution**:

```bash
# Verify config.json exists
cat config.json

# Check auth_token format
grep "auth_token" config.json
```

### Server Connection Failed

**Problem**: `Connection refused`

**Solution**:

1. Check if server is running (system tray icon)
2. Verify port in `runtime.json`
3. Check firewall settings
4. Restart server

### Permission Denied Errors

**Problem**: `Insufficient privileges`

**Solution**:

1. Run Git Bash as Administrator
2. Verify user has shutdown privilege
3. Check Windows group policies

### Protected Process Test Fails

**Problem**: Protected process was terminated

**Solution**:

- This is a CRITICAL security issue
- Review `ProcessService::PROTECTED_PROCESSES` list
- Check `isProtectedProcess()` implementation

### System Directory Test Fails

**Problem**: System directory was modified

**Solution**:

- This is a CRITICAL security issue
- Review `FileOperationService::FORBIDDEN_DIRS` list
- Check `isSystemDirectory()` implementation

### Whitelist Test Fails

**Problem**: Operation succeeded outside whitelist

**Solution**:

- This is a CRITICAL security issue
- Review `PolicyGuard::isPathAllowed()` implementation
- Check `config.json` whitelist configuration

### Shutdown Not Cancelled

**Problem**: System is shutting down

**Solution**:

```bash
# Immediately run
shutdown /a

# Or use the tool
curl -X POST http://localhost:35182/tools/call \
  -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
  -d '{"name": "abort_shutdown", "arguments": {}}'
```

## Test Coverage Summary

### Process Management

- ✅ Process priority adjustment (6 levels)
- ✅ Graceful termination
- ✅ Forced termination
- ✅ Protected process rejection
- ✅ Non-existent process handling
- ✅ Audit logging

### File Operations

- ✅ Directory creation (single and multi-level)
- ✅ File copying (with overwrite protection)
- ✅ Directory copying (recursive)
- ✅ File moving/renaming
- ✅ File deletion
- ✅ Directory deletion (recursive)
- ✅ Whitelist enforcement
- ✅ System directory protection
- ✅ Error handling
- ✅ Audit logging

### Power Management

- ✅ Shutdown scheduling
- ✅ Reboot scheduling
- ✅ Hibernate (if supported)
- ✅ Sleep (if supported)
- ✅ Forced operations
- ✅ Custom messages
- ✅ Operation cancellation
- ✅ Permission checks
- ✅ Dashboard integration
- ✅ Audit logging

## Security Validation

### Critical Security Checks

1. **Protected Processes**:
    - [ ] System processes cannot be terminated
    - [ ] Error message is clear and logged

2. **System Directories**:
    - [ ] C:\Windows cannot be modified
    - [ ] C:\Program Files cannot be modified
    - [ ] Error message is clear and logged

3. **Whitelist Enforcement**:
    - [ ] Operations outside whitelist are rejected
    - [ ] Both source and destination checked (copy/move)
    - [ ] Error message is clear and logged

4. **User Confirmation**:
    - [ ] High-risk operations require confirmation
    - [ ] Critical-risk operations require confirmation
    - [ ] User can cancel operations

5. **Audit Logging**:
    - [ ] All operations are logged
    - [ ] High-risk flag is set correctly
    - [ ] Details include all relevant information
    - [ ] Timestamps are accurate

### Security Test Checklist

Run through this checklist manually:

```bash
# 1. Try to terminate system process
# Expected: Rejected with "protected" error

# 2. Try to delete C:\Windows
# Expected: Rejected with "protected" or "not allowed" error

# 3. Try to copy file outside whitelist
# Expected: Rejected with "not allowed" error

# 4. Try to shutdown without admin (if not admin)
# Expected: Rejected with "insufficient privileges" error

# 5. Check audit log for all operations
# Expected: All operations logged with correct risk levels
```

## Performance Validation

### Expected Performance

- **Process termination**: < 100ms (graceful), < 50ms (forced)
- **Process priority**: < 50ms
- **File copy**: Depends on size, ~10MB/s minimum
- **File delete**: < 100ms per file
- **Directory creation**: < 50ms
- **Shutdown scheduling**: < 100ms
- **Shutdown cancellation**: < 100ms

### Performance Test

```bash
# Check audit log for duration_ms
grep "duration_ms" audit.log | jq '.duration_ms' | sort -n
```

## Integration Testing

### Test with Dashboard

1. Open Dashboard (if implemented)
2. Run test scripts
3. Observe real-time updates:
    - Operation list updates
    - High-risk operations marked in red
    - Shutdown countdown appears
    - Statistics update

### Test with MCP Client

If you have an MCP client (Claude Desktop, OpenClaw):

1. Configure client to connect to server
2. Ask AI to perform operations:
    - "Kill the notepad process"
    - "Copy this file to backup folder"
    - "Schedule shutdown in 5 minutes"
3. Verify operations work correctly
4. Verify user confirmations appear

## Regression Testing

After making changes, run all three test suites:

```bash
# Full test suite
../scripts/test_process_management.sh
../scripts/test_file_operations.sh
../scripts/test_power_management.sh

# Check for failures
echo "Exit code: $?"
```

## Continuous Integration

For CI/CD pipelines (future):

```bash
# Run tests in non-interactive mode
export CI=true
../scripts/test_process_management.sh --no-confirm
../scripts/test_file_operations.sh --no-confirm
# Skip power management in CI (too dangerous)
```

## Reporting Issues

When reporting test failures, include:

1. **Test script name and test number**
2. **Console output** (copy full output)
3. **Audit log entries** (relevant entries)
4. **System information**:
    - Windows version
    - Architecture (x64/x86/ARM64)
    - Admin privileges (yes/no)
5. **Server configuration** (sanitized config.json)
6. **Expected vs actual behavior**

## Next Steps

After successful testing:

1. ✅ Review audit logs for anomalies
2. ✅ Verify Dashboard displays correctly
3. ✅ Test with real MCP clients
4. ✅ Perform security audit
5. ✅ Update documentation if needed
6. ✅ Tag release as v0.3.0

## Conclusion

These test scripts provide comprehensive coverage of v0.3.0 features. All tests should pass on a properly configured system with appropriate privileges. Any failures indicate potential security issues or implementation bugs that must be addressed before release.

**Remember**: Security is paramount. Never bypass security checks to make tests pass!
