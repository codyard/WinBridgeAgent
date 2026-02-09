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
#include "support/localization_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include "utils/log_path.h"

namespace {
void LogLocalization(const std::string& message) {
    std::ofstream log(clawdesk::GetLogFilePathA("localization.log"), std::ios::app);
    if (!log.is_open()) {
        return;
    }
    log << message << std::endl;
}

std::string GetCwdA() {
    char buf[MAX_PATH] = {0};
    DWORD len = GetCurrentDirectoryA(MAX_PATH, buf);
    if (len == 0 || len >= MAX_PATH) {
        return "";
    }
    return std::string(buf, len);
}
} // namespace

using json = nlohmann::json;

LocalizationManager::LocalizationManager() {
    loadTranslations();
    currentLanguage_ = detectSystemLanguage();
    if (translations_.find(currentLanguage_) == translations_.end()) {
        currentLanguage_ = "en";
    }
    LogLocalization("init: currentLanguage=" + currentLanguage_);
}

void LocalizationManager::setLanguage(const std::string& languageCode) {
    std::string normalized = normalizeLanguageCode(languageCode);
    LogLocalization("setLanguage: requested=" + languageCode + " normalized=" + normalized);
    if (translations_.find(normalized) != translations_.end()) {
        currentLanguage_ = normalized;
        LogLocalization("setLanguage: applied=" + currentLanguage_);
    } else {
        currentLanguage_ = "en";
        LogLocalization("setLanguage: missing translations for " + normalized + ", fallback=en");
    }
}

std::string LocalizationManager::getCurrentLanguage() const {
    return currentLanguage_;
}

std::vector<LanguageInfo> LocalizationManager::getSupportedLanguages() const {
    return {
        {"zh-CN", L"简体中文", L"Simplified Chinese"},
        {"zh-TW", L"繁體中文", L"Traditional Chinese"},
        {"en", L"English", L"English"},
        {"ja", L"日本語", L"Japanese"},
        {"ko", L"한국어", L"Korean"},
        {"de", L"Deutsch", L"German"},
        {"fr", L"Français", L"French"},
        {"es", L"Español", L"Spanish"},
        {"ru", L"Русский", L"Russian"}
    };
}

std::wstring LocalizationManager::getString(const std::string& key) const {
    auto langIt = translations_.find(currentLanguage_);
    if (langIt != translations_.end()) {
        auto keyIt = langIt->second.find(key);
        if (keyIt != langIt->second.end()) {
            return keyIt->second;
        }
    }
    return getFallbackString(key);
}

std::wstring LocalizationManager::getFormattedString(const std::string& key,
                                                     const std::vector<std::wstring>& args) const {
    std::wstring format = getString(key);
    for (size_t i = 0; i < args.size(); ++i) {
        std::wstring placeholder = L"{" + std::to_wstring(i) + L"}";
        size_t pos = 0;
        while ((pos = format.find(placeholder, pos)) != std::wstring::npos) {
            format.replace(pos, placeholder.size(), args[i]);
            pos += args[i].size();
        }
    }
    return format;
}

std::string LocalizationManager::detectSystemLanguage() const {
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {0};
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
        return "en";
    }
    std::wstring wide(localeName);
    std::string locale(wide.begin(), wide.end());
    return normalizeLanguageCode(locale);
}

namespace {
std::map<std::string, std::wstring> buildDefaultEnglish() {
    return {
        {"settings.title", L"Settings"},
        {"settings.tab.language", L"Language"},
        {"settings.tab.startup", L"Startup"},
        {"settings.tab.apikey", L"API Key"},
        {"settings.tab.server", L"Server"},
        {"settings.tab.security", L"Security"},
        {"settings.tab.appearance", L"Appearance"},
        {"settings.tab.about", L"About"},
        {"settings.button.apply", L"Apply"},
        {"settings.button.ok", L"OK"},
        {"settings.button.cancel", L"Cancel"},
        {"language.select", L"Select Language:"},
        {"startup.auto", L"Start with Windows"},
        {"apikey.current", L"Current API Key:"},
        {"apikey.copy", L"Copy"},
        {"apikey.regenerate", L"Regenerate"},
        {"apikey.new", L"New API Key:"},
        {"apikey.status.valid", L"Status: valid"},
        {"apikey.status.invalid", L"Status: invalid"},
        {"server.port", L"Server Port:"},
        {"server.autoport", L"Auto-select port"},
        {"server.listen", L"Listen Address:"},
        {"server.status", L"Status: Running"},
        {"server.uptime", L"Uptime: N/A"},
        {"security.token", L"Bearer Token:"},
        {"security.token.regen", L"Regenerate Token"},
        {"security.allowed_dirs", L"Allowed directories:"},
        {"security.allowed_apps", L"Allowed apps:"},
        {"security.allowed_commands", L"Allowed commands:"},
        {"security.add", L"Add"},
        {"security.edit", L"Edit"},
        {"security.remove", L"Remove"},
        {"security.move_up", L"Up"},
        {"security.move_down", L"Down"},
        {"security.add_dir.title", L"Select directory"},
        {"security.edit_dir.title", L"Select directory"},
        {"security.add_app.title", L"Select application"},
        {"security.edit_app.title", L"Select application"},
        {"security.add_command.title", L"Add command"},
        {"security.edit_command.title", L"Edit command"},
        {"security.add_command.label", L"Command:"},
        {"security.error.select_item", L"Select an item first."},
        {"security.error.duplicate", L"Entry already exists."},
        {"security.error.empty", L"Value cannot be empty."},
        {"security.error.dir_required", L"Selected path is not a directory."},
        {"security.error.file_required", L"Selected path is not a file."},
        {"security.high_risk", L"Enable high-risk confirmations"},
        {"appearance.dashboard.autoshow", L"Show dashboard on startup"},
        {"appearance.dashboard.always_on_top", L"Dashboard always on top"},
        {"appearance.tray_icon_style", L"Tray icon style"},
        {"appearance.log_retention_days", L"Log retention days"},
        {"appearance.daemon.enabled", L"Enable daemon watchdog"},
        {"appearance.tray.normal", L"Normal"},
        {"appearance.tray.minimal", L"Minimal"},
        {"about.version", L"Version:"},
        {"about.license", L"License:"},
        {"about.expires", L"Expires:"},
        {"about.os", L"OS:"},
        {"about.arch", L"Arch:"},
        {"about.docs", L"Documentation"},
        {"about.support", L"Support"},
        {"tray.usage", L"Usage Statistics"},
        {"tray.logs", L"View Logs"},
        {"tray.dashboard", L"Dashboard"},
        {"tray.settings", L"Settings"},
        {"tray.toggle_listen", L"Toggle Listen Address"},
        {"tray.open_config", L"Open Config"},
        {"tray.about", L"About"},
        {"tray.exit", L"Exit"},
        {"tray.status.running", L"Status: Running"},
        {"tray.status.stopped", L"Status: Stopped"},
        {"tray.license.free", L"License: Free Edition"},
        {"tray.license.pro", L"License: Professional"},
        {"tray.port", L"Port"},
        {"tray.listen", L"Listen"},
        {"tray.listen.all", L"All interfaces"},
        {"tray.listen.local", L"Localhost only"},
        {"tray.tip", L"ClawDesk MCP Server - Running"},
        {"dashboard.title", L"ClawDesk MCP Server - Dashboard"},
        {"dashboard.status.default", L"Real-time monitoring - Logs are displayed below"},
        {"dashboard.high_risk_counter", L"High-Risk Ops: {0}"},
        {"dashboard.clear_logs", L"Clear Logs"},
        {"dashboard.copy_all", L"Copy All"},
        {"dashboard.total_logs", L"Total logs: {0} (max {1})"},
        {"dashboard.logs_cleared", L"Logs cleared"},
        {"dashboard.logs_copied", L"Logs copied to clipboard"},
        {"dashboard.copy_success_title", L"Success"},
        {"dashboard.cancel_shutdown", L"Cancel"},
        {"dashboard.cancel_shutdown_title", L"Cancel Shutdown"},
        {"dashboard.cancel_shutdown_message", L"Shutdown cancellation requested.\nPlease use abort_shutdown tool to confirm."},
        {"dashboard.countdown_warning", L"WARNING: System {0} scheduled in {1} seconds"},
        {"dashboard.log.received", L"Received request"},
        {"dashboard.log.completed", L"Completed successfully"},
        {"dashboard.log.error", L"Error occurred"},
        {"dashboard.log.high_risk", L"HIGH RISK OPERATION"},
        {"dashboard.log.cancel_shutdown", L"User cancelled shutdown from Dashboard"},
        {"update.https_disabled_title", L"Check for Updates"},
        {"update.https_disabled_message", L"HTTPS is not available because OpenSSL is disabled.\n\nPlease install a build with OpenSSL enabled to use update checks."}
    };
}
} // namespace

void LocalizationManager::loadTranslations() {
    translations_.clear();
    LogLocalization("loadTranslations: cwd=" + GetCwdA());
    std::ifstream file("resources/translations.json");
    if (!file.is_open()) {
        LogLocalization("loadTranslations: open failed for resources/translations.json");
        wchar_t modulePath[MAX_PATH] = {0};
        if (GetModuleFileNameW(NULL, modulePath, MAX_PATH) > 0) {
            std::wstring fullPath(modulePath);
            size_t pos = fullPath.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                std::wstring dir = fullPath.substr(0, pos);
                std::wstring rel = dir + L"\\resources\\translations.json";
                file.open(std::string(rel.begin(), rel.end()));
                LogLocalization("loadTranslations: retry with exe dir resources/translations.json");
            }
        }
    }
    if (!file.is_open()) {
        translations_["en"] = buildDefaultEnglish();
        LogLocalization("loadTranslations: failed to open translations.json, using default English");
        return;
    }

    json data;
    try {
        file >> data;
    } catch (...) {
        translations_["en"] = buildDefaultEnglish();
        LogLocalization("loadTranslations: JSON parse failed, using default English");
        return;
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        std::string lang = it.key();
        if (!it.value().is_object()) {
            continue;
        }
        std::map<std::string, std::wstring> entries;
        for (auto keyIt = it.value().begin(); keyIt != it.value().end(); ++keyIt) {
            if (!keyIt.value().is_string()) {
                continue;
            }
            entries[keyIt.key()] = utf8ToWide(keyIt.value().get<std::string>());
        }
        translations_[lang] = entries;
    }

    if (translations_.find("en") == translations_.end()) {
        translations_["en"] = buildDefaultEnglish();
    }
    LogLocalization("loadTranslations: languages loaded=" + std::to_string(translations_.size()));
}

std::wstring LocalizationManager::getFallbackString(const std::string& key) const {
    auto enIt = translations_.find("en");
    if (enIt != translations_.end()) {
        auto keyIt = enIt->second.find(key);
        if (keyIt != enIt->second.end()) {
            return keyIt->second;
        }
    }
    return utf8ToWide(key);
}

std::wstring LocalizationManager::utf8ToWide(const std::string& value) const {
    if (value.empty()) {
        return L"";
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, NULL, 0);
    if (size <= 0) {
        return L"";
    }
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &out[0], size);
    return out;
}

std::string LocalizationManager::normalizeLanguageCode(const std::string& code) const {
    std::string normalized = code;
    std::replace(normalized.begin(), normalized.end(), '_', '-');
    if (normalized.rfind("zh", 0) == 0) {
        if (normalized.find("TW") != std::string::npos || normalized.find("Hant") != std::string::npos ||
            normalized.find("HK") != std::string::npos) {
            return "zh-TW";
        }
        return "zh-CN";
    }
    if (normalized.size() >= 2) {
        normalized = normalized.substr(0, 2);
    }
    static const std::vector<std::string> supported = {
        "en", "ja", "ko", "de", "fr", "es", "ru"
    };
    if (std::find(supported.begin(), supported.end(), normalized) != supported.end()) {
        return normalized;
    }
    return "en";
}
