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
#ifndef CLAWDESK_SETTINGS_WINDOW_H
#define CLAWDESK_SETTINGS_WINDOW_H

#include <string>
#include <vector>
#include <windows.h>

class ConfigManager;
class LicenseManager;
namespace clawdesk {
class AuditLogger;
}
class LocalizationManager;

struct SettingsState {
    std::string language;
    bool autoStartup;
    std::string apiKey;
    bool apiKeyValid;
    int serverPort;
    bool autoPortSelection;
    std::string listenAddress;
    std::string bearerToken;
    std::vector<std::string> allowedDirectories;
    std::vector<std::string> allowedApplications;
    std::vector<std::string> allowedCommands;
    bool highRiskConfirmations;
    bool dashboardAutoShow;
    bool dashboardAlwaysOnTop;
    std::string trayIconStyle;
    int logRetentionDays;
    bool daemonEnabled;
    std::string appVersion;
    std::string licenseType;
    std::string licenseExpiration;
    std::string osVersion;
    std::string architecture;

    bool operator==(const SettingsState& other) const {
        return language == other.language &&
               autoStartup == other.autoStartup &&
               apiKey == other.apiKey &&
               apiKeyValid == other.apiKeyValid &&
               serverPort == other.serverPort &&
               autoPortSelection == other.autoPortSelection &&
               listenAddress == other.listenAddress &&
               bearerToken == other.bearerToken &&
               allowedDirectories == other.allowedDirectories &&
               allowedApplications == other.allowedApplications &&
               allowedCommands == other.allowedCommands &&
               highRiskConfirmations == other.highRiskConfirmations &&
               dashboardAutoShow == other.dashboardAutoShow &&
               dashboardAlwaysOnTop == other.dashboardAlwaysOnTop &&
               trayIconStyle == other.trayIconStyle &&
               logRetentionDays == other.logRetentionDays &&
               daemonEnabled == other.daemonEnabled &&
               appVersion == other.appVersion &&
               licenseType == other.licenseType &&
               licenseExpiration == other.licenseExpiration &&
               osVersion == other.osVersion &&
               architecture == other.architecture;
    }
};

inline std::string MaskSensitiveData(const std::string& data) {
    if (data.size() <= 8) {
        return std::string(data.size(), '*');
    }
    std::string masked;
    masked.reserve(data.size());
    masked.append(data.substr(0, 4));
    masked.append(data.size() - 8, '*');
    masked.append(data.substr(data.size() - 4));
    return masked;
}

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInstance,
                   HWND parentWindow,
                   ConfigManager* configManager,
                   LicenseManager* licenseManager,
                   clawdesk::AuditLogger* auditLogger,
                   LocalizationManager* localizationManager);

    bool Show();
    void Hide();

    bool LoadSettings();
    void updateLanguage();
    HWND getHwnd() const { return hwnd_; }
    bool SaveSettings();
    bool ValidateSettings();
    void RevertChanges();

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onInitDialog();
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();
    void initializeTabs();
    void updateTabTitles();
    void syncControlsToSettings();
    void syncSecurityListsFromControls();
    void updateSecurityButtonsState();
    void applyUIFont();
    void registerWindowClass();
    void createMainWindow();

    HINSTANCE hInstance_;
    HWND parentWindow_;
    HWND hwnd_;
    HWND tabControl_;
    bool dialogResult_;
    bool hasUnsavedChanges_;

    ConfigManager* configManager_;
    LicenseManager* licenseManager_;
    clawdesk::AuditLogger* auditLogger_;
    LocalizationManager* localizationManager_;
    HFONT uiFont_;

    SettingsState currentSettings_;
    SettingsState originalSettings_;
};

#endif // CLAWDESK_SETTINGS_WINDOW_H
