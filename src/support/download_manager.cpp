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
#include "support/download_manager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <winsock2.h>
#include <windows.h>

#ifdef CLAWDESK_OPENSSL_ENABLED
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <openssl/sha.h>
#else
#include <httplib.h>
#include <wincrypt.h>
#endif

namespace clawdesk {

DownloadManager::DownloadManager() {
    state_.is_downloading = false;
    state_.should_cancel = false;
    state_.downloaded_bytes = 0;
    state_.total_bytes = 0;
    state_.last_downloaded_bytes = 0;
}

DownloadManager::~DownloadManager() {
    cancelDownload();
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
}

DownloadResult DownloadManager::downloadFile(
    const std::string& url,
    const std::string& destination_path,
    const std::string& expected_sha256,
    std::function<void(const DownloadProgress&)> progress_callback
) {
    return downloadFileInternal(url, destination_path, expected_sha256, progress_callback);
}

void DownloadManager::downloadFileAsync(
    const std::string& url,
    const std::string& destination_path,
    const std::string& expected_sha256,
    std::function<void(const DownloadResult&)> completion_callback,
    std::function<void(const DownloadProgress&)> progress_callback
) {
    if (state_.is_downloading) {
        DownloadResult result;
        result.success = false;
        result.error_message = "Download already in progress";
        if (completion_callback) {
            completion_callback(result);
        }
        return;
    }

    if (download_thread_.joinable()) {
        download_thread_.join();
    }

    download_thread_ = std::thread([this, url, destination_path, expected_sha256, 
                                   completion_callback, progress_callback]() {
        DownloadResult result = downloadFileInternal(url, destination_path, 
                                                     expected_sha256, progress_callback);
        if (completion_callback) {
            completion_callback(result);
        }
    });
}

void DownloadManager::cancelDownload() {
    state_.should_cancel = true;
}

bool DownloadManager::isDownloading() const {
    return state_.is_downloading;
}

DownloadResult DownloadManager::downloadFileInternal(
    const std::string& url,
    const std::string& destination_path,
    const std::string& expected_sha256,
    std::function<void(const DownloadProgress&)> progress_callback
) {
    DownloadResult result;
    result.success = false;
    result.downloaded_file_path = destination_path;

    state_.is_downloading = true;
    state_.should_cancel = false;
    state_.downloaded_bytes = 0;
    state_.total_bytes = 0;
    state_.start_time = std::chrono::steady_clock::now();
    state_.last_update_time = state_.start_time;
    state_.last_downloaded_bytes = 0;

    try {
        updateProgress(progress_callback, "Checking disk space...");

        std::string temp_path = destination_path + ".tmp";
        
        size_t existing_size = 0;
        std::ifstream existing_file(temp_path, std::ios::binary | std::ios::ate);
        if (existing_file.good()) {
            existing_size = existing_file.tellg();
            existing_file.close();
        }

        updateProgress(progress_callback, "Connecting to server...");

        std::string scheme, host, path;
        int port = 443;
        
        if (url.substr(0, 8) == "https://") {
            scheme = "https";
            size_t host_start = 8;
            size_t path_start = url.find('/', host_start);
            if (path_start == std::string::npos) {
                host = url.substr(host_start);
                path = "/";
            } else {
                host = url.substr(host_start, path_start - host_start);
                path = url.substr(path_start);
            }
            
            size_t port_pos = host.find(':');
            if (port_pos != std::string::npos) {
                port = std::stoi(host.substr(port_pos + 1));
                host = host.substr(0, port_pos);
            }
        } else if (url.substr(0, 7) == "http://") {
            scheme = "http";
            port = 80;
            size_t host_start = 7;
            size_t path_start = url.find('/', host_start);
            if (path_start == std::string::npos) {
                host = url.substr(host_start);
                path = "/";
            } else {
                host = url.substr(host_start, path_start - host_start);
                path = url.substr(path_start);
            }
            
            size_t port_pos = host.find(':');
            if (port_pos != std::string::npos) {
                port = std::stoi(host.substr(port_pos + 1));
                host = host.substr(0, port_pos);
            }
        } else {
            result.error_message = "Invalid URL scheme";
            state_.is_downloading = false;
            return result;
        }

#ifdef CLAWDESK_OPENSSL_ENABLED
        httplib::SSLClient client(host, port);
#else
        if (scheme == "https") {
            result.error_message = "HTTPS is not available (OpenSSL disabled)";
            state_.is_downloading = false;
            return result;
        }
        httplib::Client client(host, port);
#endif
        client.set_follow_location(true);
        client.set_connection_timeout(10, 0);
        client.set_read_timeout(30, 0);
        client.set_write_timeout(30, 0);

        httplib::Headers headers = {
            {"User-Agent", "WinBridgeAgent-DownloadManager/1.0"}
        };

        if (existing_size > 0) {
            headers.emplace("Range", "bytes=" + std::to_string(existing_size) + "-");
            state_.downloaded_bytes = existing_size;
        }

        updateProgress(progress_callback, "Downloading...");

        auto res = client.Get(path.c_str(), headers,
            [this, &temp_path, existing_size, progress_callback](const char* data, size_t data_length) {
                if (state_.should_cancel) {
                    return false;
                }

                std::ofstream file(temp_path, 
                    existing_size > 0 ? (std::ios::binary | std::ios::app) : std::ios::binary);
                if (!file) {
                    return false;
                }

                file.write(data, data_length);
                file.close();

                state_.downloaded_bytes += data_length;
                
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - state_.last_update_time).count();
                
                if (elapsed >= 100) {
                    updateProgress(progress_callback, "Downloading...");
                    state_.last_update_time = now;
                    state_.last_downloaded_bytes = state_.downloaded_bytes;
                }

                return true;
            },
            [this](uint64_t current, uint64_t total) {
                if (total > 0) {
                    state_.total_bytes = total + state_.downloaded_bytes;
                }
                return true;
            }
        );

        if (state_.should_cancel) {
            result.error_message = "Download cancelled by user";
            state_.is_downloading = false;
            return result;
        }

        if (!res) {
            result.error_message = "Failed to download file: " + httplib::to_string(res.error());
            state_.is_downloading = false;
            return result;
        }

        if (res->status != 200 && res->status != 206) {
            result.error_message = "HTTP error: " + std::to_string(res->status);
            state_.is_downloading = false;
            return result;
        }

        updateProgress(progress_callback, "Finalizing download...");

        if (std::rename(temp_path.c_str(), destination_path.c_str()) != 0) {
            DeleteFileA(destination_path.c_str());
            if (std::rename(temp_path.c_str(), destination_path.c_str()) != 0) {
                result.error_message = "Failed to rename temporary file";
                state_.is_downloading = false;
                return result;
            }
        }

        result.file_size = state_.downloaded_bytes;

        if (!expected_sha256.empty()) {
            updateProgress(progress_callback, "Verifying file integrity...");
            
            std::string actual_hash = calculateSHA256(destination_path);
            result.sha256_hash = actual_hash;

            std::string expected_lower = expected_sha256;
            std::string actual_lower = actual_hash;
            std::transform(expected_lower.begin(), expected_lower.end(), 
                         expected_lower.begin(), ::tolower);
            std::transform(actual_lower.begin(), actual_lower.end(), 
                         actual_lower.begin(), ::tolower);

            if (expected_lower != actual_lower) {
                result.error_message = "SHA256 verification failed. Expected: " + 
                                     expected_sha256 + ", Got: " + actual_hash;
                DeleteFileA(destination_path.c_str());
                state_.is_downloading = false;
                return result;
            }
        } else {
            result.sha256_hash = calculateSHA256(destination_path);
        }

        updateProgress(progress_callback, "Download complete!");
        
        result.success = true;
        state_.is_downloading = false;
        return result;

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
        state_.is_downloading = false;
        return result;
    }
}

void DownloadManager::updateProgress(
    std::function<void(const DownloadProgress&)> progress_callback,
    const std::string& status_message
) {
    if (!progress_callback) {
        return;
    }

    DownloadProgress progress;
    progress.downloaded_bytes = state_.downloaded_bytes;
    progress.total_bytes = state_.total_bytes;
    progress.is_complete = false;
    progress.has_error = false;

    if (state_.total_bytes > 0) {
        progress.percentage = static_cast<int>(
            (static_cast<double>(state_.downloaded_bytes) / state_.total_bytes) * 100.0
        );
    } else {
        progress.percentage = 0;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - state_.start_time).count();
    
    if (elapsed > 0) {
        progress.speed_bytes_per_sec = static_cast<double>(state_.downloaded_bytes) / elapsed;
    } else {
        progress.speed_bytes_per_sec = 0.0;
    }

    if (!status_message.empty()) {
        progress.status_message = status_message;
    } else {
        std::ostringstream oss;
        oss << "Downloaded " << (state_.downloaded_bytes / 1024 / 1024) << " MB";
        if (state_.total_bytes > 0) {
            oss << " of " << (state_.total_bytes / 1024 / 1024) << " MB";
        }
        oss << " (" << progress.percentage << "%)";
        progress.status_message = oss.str();
    }

    progress_callback(progress);
}

std::string DownloadManager::calculateSHA256(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return "";
    }

#ifdef CLAWDESK_OPENSSL_ENABLED
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    const size_t buffer_size = 8192;
    char buffer[buffer_size];

    while (file.read(buffer, buffer_size) || file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
#else
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    const size_t buffer_size = 8192;
    char buffer[buffer_size];
    while (file.read(buffer, buffer_size) || file.gcount() > 0) {
        if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer),
                           static_cast<DWORD>(file.gcount()), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
    }

    BYTE hash[32];
    DWORD hash_len = sizeof(hash);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    for (DWORD i = 0; i < hash_len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
#endif
}

bool DownloadManager::verifySHA256(const std::string& file_path, 
                                   const std::string& expected_hash) {
    std::string actual_hash = calculateSHA256(file_path);
    if (actual_hash.empty()) {
        return false;
    }

    std::string expected_lower = expected_hash;
    std::string actual_lower = actual_hash;
    std::transform(expected_lower.begin(), expected_lower.end(), 
                 expected_lower.begin(), ::tolower);
    std::transform(actual_lower.begin(), actual_lower.end(), 
                 actual_lower.begin(), ::tolower);

    return expected_lower == actual_lower;
}

bool DownloadManager::checkDiskSpace(const std::string& path, size_t required_bytes) {
    size_t available = getAvailableDiskSpace(path);
    return available >= required_bytes;
}

size_t DownloadManager::getAvailableDiskSpace(const std::string& path) {
    ULARGE_INTEGER free_bytes_available;
    ULARGE_INTEGER total_bytes;
    ULARGE_INTEGER total_free_bytes;

    std::string drive = path.substr(0, 3);
    
    if (GetDiskFreeSpaceExA(drive.c_str(), &free_bytes_available, 
                           &total_bytes, &total_free_bytes)) {
        return static_cast<size_t>(free_bytes_available.QuadPart);
    }

    return 0;
}

bool DownloadManager::downloadChunk(
    const std::string& url,
    std::ofstream& file,
    size_t start_pos,
    size_t end_pos,
    std::function<void(const DownloadProgress&)> progress_callback
) {
    return true;
}

} // namespace clawdesk
