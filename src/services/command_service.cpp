/*
 * Copyright (C) 2026 Codyard
 *
 * This file is part of WinBridgeAgent.
 *
 * WinBridgeAgent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinBridgeAgent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinBridgeAgent. If not, see <https://www.gnu.org/licenses/\>.
 */
#include "services/command_service.h"
#include "support/config_manager.h"
#include "policy/policy_guard.h"
#include <windows.h>
#include <sstream>
#include <stdexcept>

CommandService::CommandService(ConfigManager* configManager, PolicyGuard* policyGuard)
    : configManager_(configManager), policyGuard_(policyGuard) {
}

CommandResult CommandService::executeCommand(const std::string& command,
                                             const std::vector<std::string>& args,
                                             int timeoutMs) {
    if (!validateCommand(command)) {
        throw std::runtime_error("Command not allowed");
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hOutRead = NULL;
    HANDLE hOutWrite = NULL;
    HANDLE hErrRead = NULL;
    HANDLE hErrWrite = NULL;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        throw std::runtime_error("Failed to create stdout pipe");
    }
    if (!CreatePipe(&hErrRead, &hErrWrite, &sa, 0)) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        throw std::runtime_error("Failed to create stderr pipe");
    }

    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hOutWrite;
    si.hStdError = hErrWrite;
    si.wShowWindow = SW_HIDE;

    std::ostringstream cmdLine;
    cmdLine << "cmd.exe /c " << command;
    for (const auto& arg : args) {
        cmdLine << " " << arg;
    }
    std::string cmdLineStr = cmdLine.str();
    char* cmdBuffer = _strdup(cmdLineStr.c_str());

    BOOL success = CreateProcessA(
        NULL,
        cmdBuffer,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    free(cmdBuffer);

    CloseHandle(hOutWrite);
    CloseHandle(hErrWrite);

    if (!success) {
        CloseHandle(hOutRead);
        CloseHandle(hErrRead);
        throw std::runtime_error("Failed to create process");
    }

    std::string outText;
    std::string errText;
    char buffer[4096];
    DWORD bytesRead = 0;

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    bool timedOut = (waitResult == WAIT_TIMEOUT);
    if (timedOut) {
        TerminateProcess(pi.hProcess, 1);
    }

    while (ReadFile(hOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        outText += buffer;
    }
    while (ReadFile(hErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        errText += buffer;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(hOutRead);
    CloseHandle(hErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    CommandResult result{};
    result.stdoutText = sanitizeOutput(outText, 1024 * 1024);
    result.stderrText = sanitizeOutput(errText, 1024 * 1024);
    result.exitCode = static_cast<int>(exitCode);
    result.timedOut = timedOut;
    return result;
}

bool CommandService::validateCommand(const std::string& command) {
    if (!policyGuard_) {
        return true;
    }
    return policyGuard_->isCommandAllowed(command);
}

std::string CommandService::sanitizeOutput(const std::string& output, size_t maxLength) {
    if (output.size() <= maxLength) {
        return output;
    }
    return output.substr(0, maxLength);
}
