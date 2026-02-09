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
#include "support/update_checker.h"
#include "support/download_manager.h"
#include "support/config_manager.h"
#include <sstream>
#include <algorithm>
#include <thread>
#include <regex>
#include <stdexcept>
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#ifdef CLAWDESK_OPENSSL_ENABLED
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

using json = nlohmann::json;

namespace clawdesk {

// ============================================================================
// Version 实现
// ============================================================================

Version Version::parse(const std::string& versionStr) {
    Version v;
    
    // 移除前导 'v' 或 'V'
    std::string str = versionStr;
    if (!str.empty() && (str[0] == 'v' || str[0] == 'V')) {
        str = str.substr(1);
    }
    
    // 使用正则表达式解析：major.minor.patch[-prerelease]
    std::regex versionRegex(R"((\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9\.\-]+))?)");
    std::smatch match;
    
    if (std::regex_match(str, match, versionRegex)) {
        v.major = std::stoi(match[1].str());
        v.minor = std::stoi(match[2].str());
        v.patch = std::stoi(match[3].str());
        if (match[4].matched) {
            v.prerelease = match[4].str();
        }
    }
    
    return v;
}

std::string Version::toString() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    if (!prerelease.empty()) {
        oss << "-" << prerelease;
    }
    return oss.str();
}

bool Version::operator<(const Version& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    
    // 预发布版本小于正式版本
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    if (!prerelease.empty() && other.prerelease.empty()) return true;
    
    return prerelease < other.prerelease;
}

bool Version::operator>(const Version& other) const {
    return other < *this;
}

bool Version::operator==(const Version& other) const {
    return major == other.major && 
           minor == other.minor && 
           patch == other.patch && 
           prerelease == other.prerelease;
}

bool Version::operator<=(const Version& other) const {
    return *this < other || *this == other;
}

bool Version::operator>=(const Version& other) const {
    return *this > other || *this == other;
}

// ============================================================================
// UpdateChecker 实现
// ============================================================================

UpdateChecker::UpdateChecker(const std::string& githubRepo, const std::string& currentVersion)
    : githubRepo_(githubRepo)
    , currentVersion_(Version::parse(currentVersion))
    , userAgent_("WinBridgeAgent-UpdateChecker/1.0")
    , timeoutSeconds_(10)
    , downloadManager_(std::make_unique<DownloadManager>())
    , configManager_(nullptr)
{
}

UpdateChecker::~UpdateChecker() {
    if (downloadManager_) {
        downloadManager_->cancelDownload();
    }
}

UpdateCheckResult UpdateChecker::checkForUpdates(bool includePrereleases) {
    UpdateCheckResult result;
    result.success = false;
    result.updateAvailable = false;
    result.currentVersion = currentVersion_;
    
    try {
        // 调用 GitHub API 获取最新 Release
        std::string endpoint = includePrereleases 
            ? "/repos/" + githubRepo_ + "/releases"
            : "/repos/" + githubRepo_ + "/releases/latest";
        
        std::string response = callWithRetry([this, &endpoint]() {
            return callGitHubAPI(endpoint);
        });
        
        if (response.empty()) {
            result.errorMessage = "Failed to fetch release information from GitHub";
            return result;
        }
        
        // 解析 JSON
        json j = json::parse(response);
        
        // 如果是获取所有 releases，选择第一个非草稿的
        if (j.is_array()) {
            for (const auto& item : j) {
                if (!item["draft"].get<bool>()) {
                    result.latestRelease = parseReleaseJson(item.dump());
                    break;
                }
            }
        } else {
            result.latestRelease = parseReleaseJson(response);
        }
        
        // 检查是否有更新
        if (result.latestRelease.version > currentVersion_) {
            result.updateAvailable = true;
            
            // 选择匹配的资产
            if (!selectMatchingAssets(
                result.latestRelease, 
                result.matchedAsset, 
                result.sha256Asset
            )) {
                result.errorMessage = "No matching asset found for current platform";
                return result;
            }
        }
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Exception: ") + e.what();
    }
    
    return result;
}

void UpdateChecker::checkForUpdatesAsync(
    std::function<void(const UpdateCheckResult&)> callback,
    bool includePrereleases
) {
    std::thread([this, callback, includePrereleases]() {
        UpdateCheckResult result = checkForUpdates(includePrereleases);
        callback(result);
    }).detach();
}

std::string UpdateChecker::callGitHubAPI(const std::string& endpoint) {
#ifndef CLAWDESK_OPENSSL_ENABLED
    throw std::runtime_error("HTTPS is not available (OpenSSL disabled)");
#else
    try {
        httplib::SSLClient client("api.github.com");
        client.set_connection_timeout(timeoutSeconds_);
        client.set_read_timeout(timeoutSeconds_);
        
        httplib::Headers headers = {
            {"User-Agent", userAgent_},
            {"Accept", "application/vnd.github.v3+json"}
        };
        
        auto res = client.Get(endpoint.c_str(), headers);
        
        if (res && res->status == 200) {
            return res->body;
        }
        
        return "";
        
    } catch (const std::exception& e) {
        return "";
    }
#endif
}

ReleaseInfo UpdateChecker::parseReleaseJson(const std::string& jsonStr) {
    ReleaseInfo info;
    
    try {
        json j = json::parse(jsonStr);
        
        info.tagName = j.value("tag_name", "");
        info.version = Version::parse(info.tagName);
        info.name = j.value("name", "");
        info.body = j.value("body", "");
        info.htmlUrl = j.value("html_url", "");
        info.prerelease = j.value("prerelease", false);
        info.draft = j.value("draft", false);
        info.publishedAt = j.value("published_at", "");
        
        // 解析资产
        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                ReleaseAsset a;
                a.name = asset.value("name", "");
                a.downloadUrl = asset.value("browser_download_url", "");
                a.browserUrl = asset.value("browser_download_url", "");
                a.size = asset.value("size", 0);
                a.contentType = asset.value("content_type", "");
                info.assets.push_back(a);
            }
        }
        
    } catch (const std::exception& e) {
        // 解析失败，返回空信息
    }
    
    return info;
}

bool UpdateChecker::selectMatchingAssets(
    const ReleaseInfo& release,
    ReleaseAsset& exeAsset,
    ReleaseAsset& sha256Asset
) {
    std::string arch = getCurrentArchitecture();
    
    // 支持多种文件名格式：
    // 1. WinBridgeAgent-{version}-win-{arch}.exe (推荐格式)
    // 2. WinBridgeAgent-win-{arch}.exe
    // 3. WinBridgeAgent-{arch}.exe
    std::vector<std::string> exePatterns = {
        "WinBridgeAgent-.*-win-" + arch + "\\.exe",  // 带版本号
        "WinBridgeAgent-win-" + arch + "\\.exe",     // 无版本号
        "WinBridgeAgent-" + arch + "\\.exe"          // 简化格式
    };
    
    bool foundExe = false;
    bool foundSha256 = false;
    
    // 尝试匹配可执行文件
    for (const auto& pattern : exePatterns) {
        std::regex exeRegex(pattern, std::regex::icase);
        for (const auto& asset : release.assets) {
            if (std::regex_search(asset.name, exeRegex)) {
                exeAsset = asset;
                foundExe = true;
                break;
            }
        }
        if (foundExe) break;
    }
    
    // 查找对应的 SHA256 文件
    if (foundExe) {
        std::string sha256Name = exeAsset.name + ".sha256";
        for (const auto& asset : release.assets) {
            if (asset.name == sha256Name) {
                sha256Asset = asset;
                foundSha256 = true;
                break;
            }
        }
    }
    
    // 如果没找到 SHA256 文件，尝试其他可能的命名
    if (foundExe && !foundSha256) {
        for (const auto& asset : release.assets) {
            if (asset.name.find(arch) != std::string::npos && 
                asset.name.find(".sha256") != std::string::npos) {
                sha256Asset = asset;
                foundSha256 = true;
                break;
            }
        }
    }
    
    return foundExe;
}

std::string UpdateChecker::getCurrentArchitecture() {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "x64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "x86";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "arm64";
        default:
            return "x64";  // 默认
    }
#else
    return "x64";  // 非 Windows 平台默认
#endif
}

std::string UpdateChecker::callWithRetry(
    std::function<std::string()> func,
    int maxRetries
) {
    int attempts = 0;
    std::string result;
    
    while (attempts < maxRetries) {
        result = func();
        if (!result.empty()) {
            return result;
        }
        
        attempts++;
        if (attempts < maxRetries) {
            // 指数退避：1秒、2秒、4秒...
            std::this_thread::sleep_for(
                std::chrono::seconds(1 << (attempts - 1))
            );
        }
    }
    
    return result;
}

bool UpdateChecker::downloadUpdate(
    const UpdateCheckResult& updateInfo,
    const std::string& downloadPath,
    std::function<void(const DownloadProgress&)> progressCallback
) {
    if (!updateInfo.success || !updateInfo.updateAvailable) {
        return false;
    }

    if (updateInfo.matchedAsset.downloadUrl.empty()) {
        return false;
    }

    // 检查是否需要验证签名
    bool shouldVerify = true;
    if (configManager_) {
        shouldVerify = configManager_->isUpdateVerifySignatureEnabled();
    }
    
    std::string sha256Hash;
    if (!updateInfo.sha256Asset.downloadUrl.empty()) {
        // 下载 SHA256 文件到临时位置
        std::string sha256TempPath = downloadPath + ".sha256.tmp";
        
        // 使用 DownloadManager 下载 SHA256 文件（不需要进度回调）
        DownloadResult sha256Result = downloadManager_->downloadFile(
            updateInfo.sha256Asset.downloadUrl,
            sha256TempPath,
            "",  // 不验证 SHA256 文件本身
            nullptr  // 不需要进度回调
        );
        
        if (sha256Result.success) {
            // 读取 SHA256 文件内容
            std::ifstream sha256File(sha256TempPath);
            if (sha256File.is_open()) {
                std::string sha256Content;
                std::getline(sha256File, sha256Content);
                sha256File.close();
                
                // 解析 SHA256（格式：hash *filename 或 hash）
                size_t spacePos = sha256Content.find(' ');
                if (spacePos != std::string::npos) {
                    sha256Hash = sha256Content.substr(0, spacePos);
                } else {
                    sha256Hash = sha256Content;
                }
                
                // 清理空白字符
                sha256Hash.erase(std::remove_if(sha256Hash.begin(), sha256Hash.end(), 
                    [](char c) { return std::isspace(c) || c == '\n' || c == '\r'; }), 
                    sha256Hash.end());
                
                // 删除临时文件
                DeleteFileA(sha256TempPath.c_str());
            }
        }
    } else if (shouldVerify) {
        // 如果启用了签名验证但没有 SHA256 文件，拒绝下载
        return false;
    }
    
    // 如果禁用了验证，即使没有 SHA256 也继续下载
    // 如果启用了验证，必须有 SHA256 才能继续

    size_t requiredSpace = updateInfo.matchedAsset.size + (100 * 1024 * 1024);
    if (!DownloadManager::checkDiskSpace(downloadPath, requiredSpace)) {
        return false;
    }

    DownloadResult result = downloadManager_->downloadFile(
        updateInfo.matchedAsset.downloadUrl,
        downloadPath,
        sha256Hash,
        progressCallback
    );

    return result.success;
}

void UpdateChecker::downloadUpdateAsync(
    const UpdateCheckResult& updateInfo,
    const std::string& downloadPath,
    std::function<void(bool success, const std::string& errorMessage)> completionCallback,
    std::function<void(const DownloadProgress&)> progressCallback
) {
    if (!updateInfo.success || !updateInfo.updateAvailable) {
        if (completionCallback) {
            completionCallback(false, "No update available");
        }
        return;
    }

    if (updateInfo.matchedAsset.downloadUrl.empty()) {
        if (completionCallback) {
            completionCallback(false, "No download URL available");
        }
        return;
    }

    std::thread([this, updateInfo, downloadPath, completionCallback, progressCallback]() {
        std::string sha256Hash;
        if (!updateInfo.sha256Asset.downloadUrl.empty()) {
            std::string sha256Url = updateInfo.sha256Asset.downloadUrl;
            std::string sha256Content = callGitHubAPI(sha256Url);
            if (!sha256Content.empty()) {
                size_t spacePos = sha256Content.find(' ');
                if (spacePos != std::string::npos) {
                    sha256Hash = sha256Content.substr(0, spacePos);
                } else {
                    sha256Hash = sha256Content;
                }
                
                sha256Hash.erase(std::remove_if(sha256Hash.begin(), sha256Hash.end(), 
                    [](char c) { return std::isspace(c) || c == '\n' || c == '\r'; }), 
                    sha256Hash.end());
            }
        }

        size_t requiredSpace = updateInfo.matchedAsset.size + (100 * 1024 * 1024);
        if (!DownloadManager::checkDiskSpace(downloadPath, requiredSpace)) {
            if (completionCallback) {
                completionCallback(false, "Insufficient disk space");
            }
            return;
        }

        downloadManager_->downloadFileAsync(
            updateInfo.matchedAsset.downloadUrl,
            downloadPath,
            sha256Hash,
            [completionCallback](const DownloadResult& result) {
                if (completionCallback) {
                    completionCallback(result.success, result.error_message);
                }
            },
            progressCallback
        );
    }).detach();
}

void UpdateChecker::cancelDownload() {
    if (downloadManager_) {
        downloadManager_->cancelDownload();
    }
}

bool UpdateChecker::startUpdater(
    const std::string& newExePath,
    const std::string& currentExePath
) {
    // 获取 Updater.exe 路径（与主程序同目录）
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    
    std::string updaterName = "Updater.exe";
    if (currentExePath.find("-x64") != std::string::npos) {
        updaterName = "Updater-x64.exe";
    } else if (currentExePath.find("-x86") != std::string::npos) {
        updaterName = "Updater-x86.exe";
    } else if (currentExePath.find("-arm64") != std::string::npos) {
        updaterName = "Updater-arm64.exe";
    }

    std::string updaterPath = exeDir + "\\" + updaterName;
    
    // 检查 Updater.exe 是否存在
    if (GetFileAttributesA(updaterPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // 兼容旧命名
        updaterPath = exeDir + "\\Updater.exe";
        if (GetFileAttributesA(updaterPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
    }
    
    // 获取当前进程名
    std::string processName = exePath;
    lastSlash = processName.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        processName = processName.substr(lastSlash + 1);
    }
    
    // 构建命令行参数
    // 格式：Updater.exe "new_exe_path" "target_exe_path" "process_name"
    std::string cmdLine = "\"" + updaterPath + "\" \"" + newExePath + "\" \"" + 
                         currentExePath + "\" \"" + processName + "\"";
    
    // 启动升级器
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessA(
        NULL,
        const_cast<char*>(cmdLine.c_str()),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        exeDir.c_str(),
        &si,
        &pi
    )) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    
    return false;
}

} // namespace clawdesk
