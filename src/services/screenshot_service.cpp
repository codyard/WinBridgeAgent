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
#include "services/screenshot_service.h"
#include <windows.h>
#include <gdiplus.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <cctype>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace {
int getEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = reinterpret_cast<ImageCodecInfo*>(malloc(size));
    if (!pImageCodecInfo) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[i].Clsid;
            free(pImageCodecInfo);
            return static_cast<int>(i);
        }
    }
    free(pImageCodecInfo);
    return -1;
}
} // namespace

ScreenshotService::ScreenshotService(ConfigManager* configManager)
    : configManager_(configManager) {
}

ScreenshotResult ScreenshotService::captureFullScreen() {
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOldBitmap);

    ScreenshotResult result = saveBitmapToFile(hBitmap, width, height);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return result;
}

namespace {
struct WindowSearchContext {
    std::string target;
    HWND exactMatch = NULL;
    HWND partialMatch = NULL;
};

std::string ToLower(const std::string& input) {
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

BOOL CALLBACK EnumWindowByTitleProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<WindowSearchContext*>(lParam);
    if (!ctx) {
        return TRUE;
    }
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    char title[512] = {0};
    GetWindowTextA(hwnd, title, sizeof(title));
    if (title[0] == '\0') {
        return TRUE;
    }
    std::string t = ToLower(title);
    std::string target = ToLower(ctx->target);
    if (t == target) {
        ctx->exactMatch = hwnd;
        return FALSE;
    }
    if (ctx->partialMatch == NULL && t.find(target) != std::string::npos) {
        ctx->partialMatch = hwnd;
    }
    return TRUE;
}
} // namespace

ScreenshotResult ScreenshotService::captureWindowByTitle(const std::string& title) {
    if (title.empty()) {
        throw std::runtime_error("Title is required");
    }
    WindowSearchContext ctx{title};
    EnumWindows(EnumWindowByTitleProc, reinterpret_cast<LPARAM>(&ctx));
    HWND hwnd = ctx.exactMatch ? ctx.exactMatch : ctx.partialMatch;
    if (!hwnd) {
        throw std::runtime_error("Window not found");
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        throw std::runtime_error("Failed to get window rect");
    }
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid window size");
    }

    // Ensure window is visible before capture
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }
    SetForegroundWindow(hwnd);
    Sleep(1000);

    HDC hdcWindow = GetWindowDC(hwnd);
    if (!hdcWindow) {
        throw std::runtime_error("Failed to get window DC");
    }
    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    BOOL ok = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);
    if (!ok) {
        BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);
    }
    SelectObject(hdcMem, hOldBitmap);

    ScreenshotResult result = saveBitmapToFile(hBitmap, width, height);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWindow);

    return result;
}

ScreenshotResult ScreenshotService::captureRegion(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid region size");
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, x, y, SRCCOPY);
    SelectObject(hdcMem, hOldBitmap);

    ScreenshotResult result = saveBitmapToFile(hBitmap, width, height);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return result;
}

std::string ScreenshotService::generateFilename() const {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[128];
    sprintf(filename, "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d.png",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return filename;
}

void ScreenshotService::ensureScreenshotDirectory() const {
    CreateDirectoryA("screenshots", NULL);
}

std::string ScreenshotService::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

ScreenshotResult ScreenshotService::saveBitmapToFile(HBITMAP bitmap, int width, int height) {
    ensureScreenshotDirectory();
    ScreenshotResult result{};
    result.width = width;
    result.height = height;
    result.path = generateFilename();
    result.created_at = getCurrentTimestamp();

    Bitmap image(bitmap, NULL);
    CLSID clsid;
    if (getEncoderClsid(L"image/png", &clsid) == -1) {
        throw std::runtime_error("Failed to get PNG encoder");
    }

    std::wstring widePath(result.path.begin(), result.path.end());
    Status status = image.Save(widePath.c_str(), &clsid, NULL);
    if (status != Ok) {
        throw std::runtime_error("Failed to save screenshot");
    }
    return result;
}
