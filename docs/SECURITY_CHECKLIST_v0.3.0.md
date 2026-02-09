# Security Checklist for ClawDesk MCP Server v0.3.0

## Overview

This checklist must be completed before releasing v0.3.0. All items marked as **CRITICAL** must pass. Items marked as **IMPORTANT** should pass unless there's a documented reason.

## Pre-Release Security Audit

### 1. Protected Process Security

- [ ] **CRITICAL**: System process (PID 4) cannot be terminated
- [ ] **CRITICAL**: csrss.exe cannot be terminated
- [ ] **CRITICAL**: winlogon.exe cannot be terminated
- [ ] **CRITICAL**: services.exe cannot be terminated
- [ ] **CRITICAL**: lsass.exe cannot be terminated
- [ ] **CRITICAL**: smss.exe cannot be terminated
- [ ] **CRITICAL**: wininit.exe cannot be terminated
- [ ] **IMPORTANT**: Protected process list is hardcoded (not configurable)
- [ ] **IMPORTANT**: Attempting to kill protected process returns clear error message
- [ ] **IMPORTANT**: Protected process attempts are logged in audit log

**Test Command**:

```bash
../scripts/test_security.sh  # Section 1
```

**Verification**:

- All attempts to terminate protected processes should fail
- Error message should mention "protected" or "access denied"
- Audit log should contain denied attempts

---

### 2. System Directory Protection

- [ ] **CRITICAL**: C:\Windows cannot be deleted
- [ ] **CRITICAL**: C:\Program Files cannot be deleted
- [ ] **CRITICAL**: C:\Program Files (x86) cannot be deleted
- [ ] **CRITICAL**: C:\ProgramData\Microsoft cannot be deleted
- [ ] **CRITICAL**: Cannot create files/directories in system directories
- [ ] **CRITICAL**: Cannot copy files to system directories
- [ ] **CRITICAL**: Cannot move files to system directories
- [ ] **IMPORTANT**: System directory list is hardcoded (not configurable)
- [ ] **IMPORTANT**: Attempting to modify system directory returns clear error message
- [ ] **IMPORTANT**: System directory attempts are logged in audit log

**Test Command**:

```bash
../scripts/test_security.sh  # Section 2
```

**Verification**:

- All attempts to modify system directories should fail
- Error message should mention "protected" or "system directory"
- No files should be created in system directories

---

### 3. Whitelist Enforcement

- [ ] **CRITICAL**: Operations outside `allowed_dirs` are rejected
- [ ] **CRITICAL**: Both source and destination paths are checked (copy/move)
- [ ] **CRITICAL**: Path traversal attacks are blocked (../ sequences)
- [ ] **IMPORTANT**: Whitelist is loaded from config.json
- [ ] **IMPORTANT**: Empty whitelist blocks all operations
- [ ] **IMPORTANT**: Whitelist violations return clear error message
- [ ] **IMPORTANT**: Whitelist violations are logged in audit log

**Test Command**:

```bash
../scripts/test_security.sh  # Section 3
```

**Verification**:

- Operations outside whitelist should fail
- Path traversal attempts should fail
- Error message should mention "not allowed" or "whitelist"

---

### 4. Authentication & Authorization

- [ ] **CRITICAL**: Requests without auth token are rejected (401)
- [ ] **CRITICAL**: Requests with invalid auth token are rejected (401)
- [ ] **CRITICAL**: Requests with malformed auth header are rejected (401)
- [ ] **CRITICAL**: Auth token is randomly generated on first run
- [ ] **CRITICAL**: Auth token is stored securely in config.json
- [ ] **IMPORTANT**: Auth token is not logged in audit log
- [ ] **IMPORTANT**: Auth token is not exposed in error messages
- [ ] **IMPORTANT**: Failed auth attempts are logged

**Test Command**:

```bash
../scripts/test_security.sh  # Section 4
```

**Verification**:

- All unauthenticated requests should return 401
- Auth token should be UUID or similar random string
- Check audit log for failed auth attempts

---

### 5. Input Validation

- [ ] **CRITICAL**: SQL injection attempts are handled safely
- [ ] **CRITICAL**: Command injection attempts are blocked
- [ ] **CRITICAL**: Path traversal attacks are blocked
- [ ] **CRITICAL**: Buffer overflow protection (long inputs rejected)
- [ ] **IMPORTANT**: Negative PIDs are rejected
- [ ] **IMPORTANT**: Zero PID is rejected
- [ ] **IMPORTANT**: Invalid priority levels are rejected
- [ ] **IMPORTANT**: Invalid power actions are rejected
- [ ] **IMPORTANT**: Negative delays are rejected
- [ ] **IMPORTANT**: Malformed JSON is rejected

**Test Command**:

```bash
../scripts/test_security.sh  # Section 5
```

**Verification**:

- All malicious inputs should be rejected
- No code execution from user input
- No buffer overflows or crashes

---

### 6. Audit Logging

- [ ] **CRITICAL**: All high-risk operations are logged
- [ ] **CRITICAL**: All failed operations are logged
- [ ] **CRITICAL**: Audit log contains required fields (time, tool, risk, result, duration_ms)
- [ ] **CRITICAL**: Audit log uses JSON Lines format
- [ ] **CRITICAL**: Audit log timestamps are UTC ISO 8601
- [ ] **IMPORTANT**: High-risk operations have `high_risk: true` flag
- [ ] **IMPORTANT**: Operation details are logged (PID, path, action, etc.)
- [ ] **IMPORTANT**: Audit log is append-only
- [ ] **IMPORTANT**: Audit log rotation is implemented
- [ ] **IMPORTANT**: Old audit logs are cleaned up (retention policy)

**Test Command**:

```bash
../scripts/test_security.sh  # Section 6
tail -50 audit.log | jq '.'
```

**Verification**:

- Check audit.log for recent operations
- Verify JSON format is valid
- Verify all required fields are present
- Verify high-risk flag is set correctly

---

### 7. User Confirmation

- [ ] **CRITICAL**: High-risk operations require user confirmation
- [ ] **CRITICAL**: Critical-risk operations require user confirmation
- [ ] **IMPORTANT**: Confirmation dialog shows operation details
- [ ] **IMPORTANT**: Confirmation dialog shows risk level
- [ ] **IMPORTANT**: User can cancel operations
- [ ] **IMPORTANT**: Cancelled operations are logged
- [ ] **IMPORTANT**: Confirmation timeout is reasonable (30-60 seconds)

**Manual Test**:

1. Run `../scripts/test_process_management.sh`
2. Verify MessageBox appears for kill_process
3. Click "Cancel" and verify operation is cancelled
4. Check audit log for cancelled operation

**Verification**:

- MessageBox should appear for high/critical risk operations
- MessageBox should show tool name and parameters
- Cancelling should prevent operation execution

---

### 8. Dashboard Security

- [ ] **IMPORTANT**: High-risk operations are marked in red
- [ ] **IMPORTANT**: Shutdown countdown is displayed
- [ ] **IMPORTANT**: Cancel button works for scheduled shutdowns
- [ ] **IMPORTANT**: Dashboard shows operation history
- [ ] **IMPORTANT**: Dashboard does not expose sensitive data (auth tokens, etc.)

**Manual Test**:

1. Open Dashboard
2. Run `../scripts/test_power_management.sh`
3. Verify countdown appears
4. Click cancel button
5. Verify shutdown is cancelled

---

### 9. Configuration Security

- [ ] **CRITICAL**: config.json has appropriate file permissions
- [ ] **CRITICAL**: Auth token is not world-readable
- [ ] **IMPORTANT**: Config file validation on load
- [ ] **IMPORTANT**: Invalid config is rejected with clear error
- [ ] **IMPORTANT**: Config changes require server restart (or explicit reload)
- [ ] **IMPORTANT**: Default config is secure (minimal whitelist)

**Manual Test**:

1. Check config.json file permissions
2. Try to load invalid config
3. Verify error message is clear

---

### 10. Network Security

- [ ] **CRITICAL**: Server listens on 127.0.0.1 only (not 0.0.0.0)
- [ ] **IMPORTANT**: HTTPS is recommended (document in README)
- [ ] **IMPORTANT**: CORS is properly configured
- [ ] **IMPORTANT**: Rate limiting is implemented (or documented as future work)

**Test Command**:

```bash
# Check listening address
netstat -an | grep 35182

# Should show 127.0.0.1:35182, not 0.0.0.0:35182
```

**Verification**:

- Server should only be accessible from localhost
- External connections should be rejected

---

### 11. Error Handling

- [ ] **CRITICAL**: Errors do not expose sensitive information
- [ ] **CRITICAL**: Errors do not expose stack traces to client
- [ ] **CRITICAL**: Errors do not expose file paths (except in allowed_dirs)
- [ ] **IMPORTANT**: Error messages are user-friendly
- [ ] **IMPORTANT**: Detailed errors are logged (not sent to client)
- [ ] **IMPORTANT**: Exceptions are caught and handled gracefully

**Manual Test**:

1. Trigger various errors (invalid input, permission denied, etc.)
2. Check error responses
3. Verify no sensitive data is exposed

---

### 12. Privilege Management

- [ ] **CRITICAL**: Server runs with minimum required privileges
- [ ] **CRITICAL**: SE_SHUTDOWN_NAME privilege is only enabled when needed
- [ ] **IMPORTANT**: Privilege escalation is not possible
- [ ] **IMPORTANT**: Admin operations are clearly documented
- [ ] **IMPORTANT**: Non-admin operations work without elevation

**Manual Test**:

1. Run server as normal user
2. Test non-admin operations (should work)
3. Test admin operations (should fail with clear message)
4. Run server as admin
5. Test admin operations (should work)

---

### 13. Code Security

- [ ] **CRITICAL**: No hardcoded credentials or secrets
- [ ] **CRITICAL**: No SQL injection vulnerabilities
- [ ] **CRITICAL**: No command injection vulnerabilities
- [ ] **CRITICAL**: No buffer overflow vulnerabilities
- [ ] **IMPORTANT**: Input validation on all user inputs
- [ ] **IMPORTANT**: Output encoding on all outputs
- [ ] **IMPORTANT**: Memory is properly managed (no leaks)
- [ ] **IMPORTANT**: Resources are properly cleaned up

**Code Review**:

- Review all user input handling
- Review all system calls
- Review all file operations
- Review all process operations
- Use static analysis tools if available

---

### 14. Dependency Security

- [ ] **IMPORTANT**: All third-party libraries are up-to-date
- [ ] **IMPORTANT**: No known vulnerabilities in dependencies
- [ ] **IMPORTANT**: Dependencies are from trusted sources
- [ ] **IMPORTANT**: Dependencies are vendored (not downloaded at runtime)

**Check Dependencies**:

- cpp-httplib: Check for latest version
- nlohmann/json: Check for latest version
- stb_image_write: Check for latest version

---

### 15. Documentation Security

- [ ] **IMPORTANT**: Security best practices are documented
- [ ] **IMPORTANT**: Whitelist configuration is documented
- [ ] **IMPORTANT**: Admin requirements are documented
- [ ] **IMPORTANT**: Security limitations are documented
- [ ] **IMPORTANT**: Incident response is documented

**Review Documents**:

- README.md
- docs/ProcessManagement.md
- docs/FileOperations.md
- docs/PowerManagement.md
- docs/Authentication.md

---

## Security Test Execution

### Run All Security Tests

```bash
# Run comprehensive security test
../scripts/test_security.sh

# Expected result: All tests pass, no critical failures
```

### Manual Security Review

1. **Code Review**:
    - Review `src/services/process_service.cpp`
    - Review `src/services/file_operation_service.cpp`
    - Review `src/services/power_service.cpp`
    - Review `src/policy/policy_guard.cpp`

2. **Configuration Review**:
    - Review `config.json`
    - Review `resources/config.template.json`
    - Verify default whitelist is minimal

3. **Audit Log Review**:
    - Review recent audit.log entries
    - Verify all operations are logged
    - Verify high-risk flag is set correctly

4. **Dashboard Review**:
    - Open Dashboard
    - Verify high-risk operations are marked
    - Verify no sensitive data is exposed

---

## Security Incident Response

### If Security Test Fails

1. **STOP**: Do not release v0.3.0
2. **Document**: Record the failure details
3. **Fix**: Implement fix for the security issue
4. **Test**: Re-run security tests
5. **Review**: Code review the fix
6. **Verify**: Verify fix does not break other functionality

### If Critical Failure Detected

1. **IMMEDIATE**: Stop all testing
2. **ALERT**: Notify development team
3. **ISOLATE**: Do not deploy to production
4. **FIX**: Implement emergency fix
5. **AUDIT**: Review all related code
6. **TEST**: Comprehensive re-testing

---

## Sign-Off

### Security Reviewer

- [ ] I have reviewed all security tests
- [ ] I have verified all critical items pass
- [ ] I have reviewed the code for security issues
- [ ] I have verified the audit log is working correctly
- [ ] I have verified user confirmations are working
- [ ] I have verified whitelist enforcement is working
- [ ] I have verified protected process/directory lists are correct
- [ ] I recommend v0.3.0 for release

**Reviewer Name**: ****\*\*\*\*****\_\_\_****\*\*\*\*****

**Date**: ****\*\*\*\*****\_\_\_****\*\*\*\*****

**Signature**: ****\*\*\*\*****\_\_\_****\*\*\*\*****

---

## Release Approval

- [ ] All critical security tests pass
- [ ] All important security tests pass (or documented exceptions)
- [ ] Security reviewer has signed off
- [ ] Code review completed
- [ ] Documentation reviewed
- [ ] No known security vulnerabilities

**Approved for Release**: YES / NO

**Approver Name**: ****\*\*\*\*****\_\_\_****\*\*\*\*****

**Date**: ****\*\*\*\*****\_\_\_****\*\*\*\*****

---

## Post-Release Monitoring

After release, monitor for:

1. **Audit Log Anomalies**: Unusual patterns or high error rates
2. **Failed Auth Attempts**: Potential brute force attacks
3. **Whitelist Violations**: Attempts to access unauthorized paths
4. **Protected Process Attempts**: Attempts to kill system processes
5. **System Directory Attempts**: Attempts to modify system directories

Set up alerts for:

- High rate of failed operations
- Multiple failed auth attempts
- Attempts to access protected resources
- Unusual power management operations

---

## Security Updates

If security issues are discovered post-release:

1. **Assess Severity**: Critical, High, Medium, Low
2. **Develop Fix**: Implement security patch
3. **Test Fix**: Run full security test suite
4. **Release Patch**: Emergency release if critical
5. **Notify Users**: Security advisory if needed
6. **Update Documentation**: Document the issue and fix

---

## Conclusion

This security checklist ensures that ClawDesk MCP Server v0.3.0 meets security requirements before release. All critical items must pass. Any failures must be addressed before release.

**Security is not optional. When in doubt, fail secure.**
