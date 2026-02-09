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
#ifndef CLAWDESK_AUTO_STARTUP_MANAGER_H
#define CLAWDESK_AUTO_STARTUP_MANAGER_H

#include <string>

class AutoStartupManager {
public:
    bool isAutoStartupEnabled() const;
    bool enableAutoStartup(const std::wstring& executablePath);
    bool disableAutoStartup();
    std::string getLastError() const;

private:
    bool setRegistryValue(const std::wstring& value);
    bool deleteRegistryValue();
    std::wstring getRegistryValue() const;

    mutable std::string lastError_;
    static const wchar_t* REGISTRY_KEY;
    static const wchar_t* REGISTRY_VALUE_NAME;
};

#endif // CLAWDESK_AUTO_STARTUP_MANAGER_H
