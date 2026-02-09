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
#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace clawdesk {

struct DownloadProgress {
    size_t downloaded_bytes;
    size_t total_bytes;
    double speed_bytes_per_sec;
    int percentage;
    std::string status_message;
    bool is_complete;
    bool has_error;
    std::string error_message;
};

struct DownloadResult {
    bool success;
    std::string error_message;
    std::string downloaded_file_path;
    std::string sha256_hash;
    size_t file_size;
};

class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();

    DownloadResult downloadFile(
        const std::string& url,
        const std::string& destination_path,
        const std::string& expected_sha256 = "",
        std::function<void(const DownloadProgress&)> progress_callback = nullptr
    );

    void downloadFileAsync(
        const std::string& url,
        const std::string& destination_path,
        const std::string& expected_sha256,
        std::function<void(const DownloadResult&)> completion_callback,
        std::function<void(const DownloadProgress&)> progress_callback = nullptr
    );

    void cancelDownload();
    bool isDownloading() const;

    static std::string calculateSHA256(const std::string& file_path);
    static bool verifySHA256(const std::string& file_path, const std::string& expected_hash);
    static bool checkDiskSpace(const std::string& path, size_t required_bytes);
    static size_t getAvailableDiskSpace(const std::string& path);

private:
    struct DownloadState {
        std::atomic<bool> is_downloading;
        std::atomic<bool> should_cancel;
        std::atomic<size_t> downloaded_bytes;
        std::atomic<size_t> total_bytes;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update_time;
        size_t last_downloaded_bytes;
        std::mutex mutex;
    };

    DownloadResult downloadFileInternal(
        const std::string& url,
        const std::string& destination_path,
        const std::string& expected_sha256,
        std::function<void(const DownloadProgress&)> progress_callback
    );

    bool downloadWithResume(
        const std::string& url,
        const std::string& destination_path,
        std::function<void(const DownloadProgress&)> progress_callback
    );

    void updateProgress(
        std::function<void(const DownloadProgress&)> progress_callback,
        const std::string& status_message = ""
    );

    bool downloadChunk(
        const std::string& url,
        std::ofstream& file,
        size_t start_pos,
        size_t end_pos,
        std::function<void(const DownloadProgress&)> progress_callback
    );

    DownloadState state_;
    std::thread download_thread_;
};

} // namespace clawdesk

#endif // DOWNLOAD_MANAGER_H
