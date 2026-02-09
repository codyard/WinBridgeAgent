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
#ifndef CLAWDESK_LOCALIZATION_MANAGER_H
#define CLAWDESK_LOCALIZATION_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <windows.h>

struct LanguageInfo {
    std::string code;
    std::wstring nativeName;
    std::wstring englishName;
};

class LocalizationManager {
public:
    LocalizationManager();

    void setLanguage(const std::string& languageCode);
    std::string getCurrentLanguage() const;
    std::vector<LanguageInfo> getSupportedLanguages() const;

    std::wstring getString(const std::string& key) const;
    std::wstring getFormattedString(const std::string& key,
                                    const std::vector<std::wstring>& args) const;

    std::string detectSystemLanguage() const;

private:
    void loadTranslations();
    std::wstring getFallbackString(const std::string& key) const;
    std::wstring utf8ToWide(const std::string& value) const;
    std::string normalizeLanguageCode(const std::string& code) const;

    std::string currentLanguage_;
    std::map<std::string, std::map<std::string, std::wstring>> translations_;
};

#endif // CLAWDESK_LOCALIZATION_MANAGER_H
