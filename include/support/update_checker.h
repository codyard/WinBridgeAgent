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
#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

class ConfigManager;

namespace clawdesk {

// 前向声明
struct DownloadProgress;
class DownloadManager;

// 语义化版本结构
struct Version {
    int major;
    int minor;
    int patch;
    std::string prerelease;  // 预发布版本标识（如 "beta.1"）
    
    Version() : major(0), minor(0), patch(0) {}
    Version(int maj, int min, int pat, const std::string& pre = "")
        : major(maj), minor(min), patch(pat), prerelease(pre) {}
    
    // 从字符串解析版本号（如 "0.5.0" 或 "0.5.0-beta.1"）
    static Version parse(const std::string& versionStr);
    
    // 转换为字符串
    std::string toString() const;
    
    // 比较运算符
    bool operator<(const Version& other) const;
    bool operator>(const Version& other) const;
    bool operator==(const Version& other) const;
    bool operator<=(const Version& other) const;
    bool operator>=(const Version& other) const;
};

// Release 资产信息
struct ReleaseAsset {
    std::string name;           // 文件名
    std::string downloadUrl;    // 下载 URL
    std::string browserUrl;     // 浏览器 URL
    size_t size;                // 文件大小（字节）
    std::string contentType;    // MIME 类型
};

// Release 信息
struct ReleaseInfo {
    std::string tagName;        // 标签名（如 "v0.5.0"）
    Version version;            // 解析后的版本号
    std::string name;           // Release 名称
    std::string body;           // Release 说明（Markdown）
    std::string htmlUrl;        // GitHub Release 页面 URL
    bool prerelease;            // 是否为预发布版本
    bool draft;                 // 是否为草稿
    std::string publishedAt;    // 发布时间（ISO 8601）
    std::vector<ReleaseAsset> assets;  // 资产列表
};

// 更新检查结果
struct UpdateCheckResult {
    bool success;               // 是否成功
    bool updateAvailable;       // 是否有更新
    std::string errorMessage;   // 错误信息
    ReleaseInfo latestRelease;  // 最新版本信息
    Version currentVersion;     // 当前版本
    ReleaseAsset matchedAsset;  // 匹配的资产（可执行文件）
    ReleaseAsset sha256Asset;   // SHA256 文件资产
};

// UpdateChecker 类
class UpdateChecker {
public:
    UpdateChecker(const std::string& githubRepo, const std::string& currentVersion);
    ~UpdateChecker();
    
    // 检查更新（同步）
    UpdateCheckResult checkForUpdates(bool includePrereleases = false);
    
    // 检查更新（异步）
    void checkForUpdatesAsync(
        std::function<void(const UpdateCheckResult&)> callback,
        bool includePrereleases = false
    );
    
    // 下载更新（同步，带进度回调）
    bool downloadUpdate(
        const UpdateCheckResult& updateInfo,
        const std::string& downloadPath,
        std::function<void(const DownloadProgress&)> progressCallback = nullptr
    );
    
    // 下载更新（异步）
    void downloadUpdateAsync(
        const UpdateCheckResult& updateInfo,
        const std::string& downloadPath,
        std::function<void(bool success, const std::string& errorMessage)> completionCallback,
        std::function<void(const DownloadProgress&)> progressCallback = nullptr
    );
    
    // 取消下载
    void cancelDownload();
    
    // 启动升级器并退出主程序
    bool startUpdater(
        const std::string& newExePath,
        const std::string& currentExePath
    );
    
    // 获取当前版本
    Version getCurrentVersion() const { return currentVersion_; }
    
    // 设置 User-Agent
    void setUserAgent(const std::string& userAgent) { userAgent_ = userAgent; }
    
    // 设置超时时间（秒）
    void setTimeout(int seconds) { timeoutSeconds_ = seconds; }
    
    // 设置配置管理器（用于读取 verify_signature 等配置）
    void setConfigManager(::ConfigManager* configManager) { configManager_ = configManager; }

private:
    // GitHub API 调用
    std::string callGitHubAPI(const std::string& endpoint);
    
    // 解析 Release JSON
    ReleaseInfo parseReleaseJson(const std::string& json);
    
    // 选择匹配的资产（根据当前平台）
    bool selectMatchingAssets(
        const ReleaseInfo& release,
        ReleaseAsset& exeAsset,
        ReleaseAsset& sha256Asset
    );
    
    // 获取当前平台架构
    std::string getCurrentArchitecture();
    
    // 重试机制
    std::string callWithRetry(
        std::function<std::string()> func,
        int maxRetries = 3
    );

private:
    std::string githubRepo_;        // GitHub 仓库（如 "owner/repo"）
    Version currentVersion_;        // 当前版本
    std::string userAgent_;         // User-Agent
    int timeoutSeconds_;            // 超时时间（秒）
    std::unique_ptr<DownloadManager> downloadManager_;  // 下载管理器
    ::ConfigManager* configManager_;  // 配置管理器（用于读取 verify_signature）
};

} // namespace clawdesk

#endif // UPDATE_CHECKER_H
