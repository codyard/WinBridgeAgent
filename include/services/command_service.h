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
#ifndef CLAWDESK_COMMAND_SERVICE_H
#define CLAWDESK_COMMAND_SERVICE_H

#include <string>
#include <vector>

class ConfigManager;
class PolicyGuard;

struct CommandResult {
    std::string stdoutText;
    std::string stderrText;
    int exitCode;
    bool timedOut;
};

class CommandService {
public:
    CommandService(ConfigManager* configManager, PolicyGuard* policyGuard);

    CommandResult executeCommand(const std::string& command,
                                 const std::vector<std::string>& args,
                                 int timeoutMs = 30000);

private:
    bool validateCommand(const std::string& command);
    std::string sanitizeOutput(const std::string& output, size_t maxLength);

    ConfigManager* configManager_;
    PolicyGuard* policyGuard_;
};

#endif // CLAWDESK_COMMAND_SERVICE_H
