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
#include "support/auto_startup_manager.h"
#include <windows.h>

const wchar_t* AutoStartupManager::REGISTRY_KEY =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* AutoStartupManager::REGISTRY_VALUE_NAME = L"WinBridgeAgent";

bool AutoStartupManager::isAutoStartupEnabled() const {
    std::wstring value = getRegistryValue();
    return !value.empty();
}

bool AutoStartupManager::enableAutoStartup(const std::wstring& executablePath) {
    return setRegistryValue(executablePath);
}

bool AutoStartupManager::disableAutoStartup() {
    return deleteRegistryValue();
}

std::string AutoStartupManager::getLastError() const {
    return lastError_;
}

bool AutoStartupManager::setRegistryValue(const std::wstring& value) {
    HKEY hKey = NULL;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        lastError_ = "Failed to open registry key";
        return false;
    }

    result = RegSetValueExW(
        hKey,
        REGISTRY_VALUE_NAME,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))
    );
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        lastError_ = "Failed to set registry value";
        return false;
    }

    lastError_.clear();
    return true;
}

bool AutoStartupManager::deleteRegistryValue() {
    HKEY hKey = NULL;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        lastError_ = "Failed to open registry key";
        return false;
    }

    result = RegDeleteValueW(hKey, REGISTRY_VALUE_NAME);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        lastError_ = "Failed to delete registry value";
        return false;
    }

    lastError_.clear();
    return true;
}

std::wstring AutoStartupManager::getRegistryValue() const {
    HKEY hKey = NULL;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return L"";
    }

    DWORD type = 0;
    DWORD size = 0;
    result = RegGetValueW(
        hKey,
        NULL,
        REGISTRY_VALUE_NAME,
        RRF_RT_REG_SZ,
        &type,
        NULL,
        &size
    );
    if (result != ERROR_SUCCESS || size == 0) {
        RegCloseKey(hKey);
        return L"";
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');
    result = RegGetValueW(
        hKey,
        NULL,
        REGISTRY_VALUE_NAME,
        RRF_RT_REG_SZ,
        &type,
        &value[0],
        &size
    );
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        return L"";
    }

    if (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}
