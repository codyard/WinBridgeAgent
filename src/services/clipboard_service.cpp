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
#include "services/clipboard_service.h"
#include <windows.h>
#include <stdexcept>

namespace {
std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return "";
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) {
        return "";
    }
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &out[0], size, NULL, NULL);
    return out;
}

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return L"";
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (size <= 0) {
        return L"";
    }
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], size);
    return out;
}
} // namespace

std::string ClipboardService::readText() {
    if (!openClipboard()) {
        throw std::runtime_error("Failed to open clipboard");
    }

    std::string result;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* data = static_cast<wchar_t*>(GlobalLock(hData));
            if (data) {
                result = wideToUtf8(std::wstring(data));
                GlobalUnlock(hData);
            }
        }
    } else if (IsClipboardFormatAvailable(CF_TEXT)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* data = static_cast<char*>(GlobalLock(hData));
            if (data) {
                result = std::string(data);
                GlobalUnlock(hData);
            }
        }
    }

    closeClipboard();
    return truncateIfNeeded(result, 10 * 1024 * 1024);
}

void ClipboardService::writeText(const std::string& text) {
    if (!openClipboard()) {
        throw std::runtime_error("Failed to open clipboard");
    }

    EmptyClipboard();

    std::wstring wide = utf8ToWide(text);
    size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        closeClipboard();
        throw std::runtime_error("Failed to allocate clipboard memory");
    }

    void* ptr = GlobalLock(hMem);
    memcpy(ptr, wide.c_str(), bytes);
    GlobalUnlock(hMem);

    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
        closeClipboard();
        throw std::runtime_error("Failed to set clipboard data");
    }

    closeClipboard();
}

std::string ClipboardService::truncateIfNeeded(const std::string& text, size_t maxLength) {
    if (text.size() <= maxLength) {
        return text;
    }
    return text.substr(0, maxLength);
}

bool ClipboardService::openClipboard() {
    return OpenClipboard(NULL) != 0;
}

void ClipboardService::closeClipboard() {
    CloseClipboard();
}
