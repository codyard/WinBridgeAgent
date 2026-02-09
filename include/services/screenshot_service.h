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
#ifndef CLAWDESK_SCREENSHOT_SERVICE_H
#define CLAWDESK_SCREENSHOT_SERVICE_H

#include <string>
#include <windows.h>

class ConfigManager;

struct ScreenshotResult {
    std::string path;
    int width;
    int height;
    std::string created_at;
};

class ScreenshotService {
public:
    explicit ScreenshotService(ConfigManager* configManager);

    ScreenshotResult captureFullScreen();
    ScreenshotResult captureWindowByTitle(const std::string& title);
    ScreenshotResult captureRegion(int x, int y, int width, int height);

private:
    std::string generateFilename() const;
    void ensureScreenshotDirectory() const;
    std::string getCurrentTimestamp() const;
    ScreenshotResult saveBitmapToFile(HBITMAP bitmap, int width, int height);

    ConfigManager* configManager_;
};

#endif // CLAWDESK_SCREENSHOT_SERVICE_H
