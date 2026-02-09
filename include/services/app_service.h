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
#ifndef CLAWDESK_APP_SERVICE_H
#define CLAWDESK_APP_SERVICE_H

#include <string>
#include <windows.h>

class ConfigManager;
class PolicyGuard;

struct LaunchResult {
    bool success;
    DWORD pid;
    std::string error;
};

struct CloseResult {
    bool success;
    std::string error;
};

class AppService {
public:
    AppService(ConfigManager* configManager, PolicyGuard* policyGuard);

    LaunchResult launchApp(const std::string& appName);
    CloseResult closeApp(const std::string& appNameOrPid);

private:
    std::string resolveAppPath(const std::string& appName);
    bool terminateProcess(DWORD pid);
    DWORD findProcessByName(const std::string& name);

    ConfigManager* configManager_;
    PolicyGuard* policyGuard_;
};

#endif // CLAWDESK_APP_SERVICE_H
