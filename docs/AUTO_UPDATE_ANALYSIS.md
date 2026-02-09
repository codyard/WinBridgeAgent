# 自动升级机制分析与建议

## 设计评估

你的 `upgrade.md` 设计非常完善！主要优点：

### ✅ 优点

1. **安全性强**
    - SHA256 完整性校验
    - Authenticode 代码签名验证
    - 证书指纹固定（Certificate Pinning）

2. **可靠性高**
    - 完整的状态机设计
    - 自动回滚机制
    - 备份旧版本

3. **可追溯**
    - 审计日志记录所有升级事件
    - 支持查看更新日志

4. **灵活性好**
    - 支持 Stable/Beta 通道
    - 可配置检查频率
    - 支持手动/自动升级

5. **用户友好**
    - 托盘菜单集成
    - 清晰的状态提示

## 实施建议

### 1. 分阶段实施

#### Phase 1: 基础版本检查（v0.5.0）

- [ ] GitHub API 集成
- [ ] 版本比较逻辑
- [ ] 托盘菜单"检查更新"
- [ ] 显示更新通知

#### Phase 2: 下载和校验（v0.6.0）

- [ ] 下载管理器
- [ ] SHA256 校验
- [ ] 进度显示

#### Phase 3: 自动升级（v0.7.0）

- [ ] 外置升级器（Updater.exe）
- [ ] 备份机制
- [ ] 应用更新

#### Phase 4: 高级功能（v0.8.0）

- [ ] Authenticode 签名验证
- [ ] 自动回滚
- [ ] Beta 通道支持

### 2. 技术实现建议

#### 2.1 版本检查服务

创建一个独立的 `UpdateChecker` 类：

```cpp
class UpdateChecker {
public:
    struct VersionInfo {
        std::string version;
        std::string download_url;
        std::string sha256_url;
        std::string release_notes_url;
        std::string published_at;
        bool is_prerelease;
    };

    UpdateChecker(const std::string& owner, const std::string& repo);

    // 检查更新
    std::optional<VersionInfo> checkForUpdates(bool includeBeta = false);

    // 比较版本
    bool isNewerVersion(const std::string& current, const std::string& latest);

private:
    std::string owner_;
    std::string repo_;
};
```

#### 2.2 下载管理器

```cpp
class DownloadManager {
public:
    struct DownloadProgress {
        size_t total_bytes;
        size_t downloaded_bytes;
        double percentage;
        double speed_mbps;
    };

    using ProgressCallback = std::function<void(const DownloadProgress&)>;

    // 下载文件
    bool download(const std::string& url,
                  const std::string& output_path,
                  ProgressCallback callback = nullptr);

    // 验证 SHA256
    bool verifySHA256(const std::string& file_path,
                      const std::string& expected_hash);
};
```

#### 2.3 外置升级器

创建一个独立的 `Updater.exe`：

```cpp
// Updater.exe 的主要逻辑
int main(int argc, char* argv[]) {
    // 参数：旧版本路径、新版本路径、主程序路径

    // 1. 等待主程序退出
    waitForProcessExit(mainProcessPID);

    // 2. 备份旧版本
    backupOldVersion(oldPath, backupPath);

    // 3. 替换新版本
    replaceExecutable(newPath, mainPath);

    // 4. 启动新版本
    launchNewVersion(mainPath);

    // 5. 清理临时文件
    cleanup();

    return 0;
}
```

#### 2.4 配置管理

扩展 `ConfigManager` 支持更新配置：

```cpp
class ConfigManager {
public:
    // 更新配置
    struct UpdateConfig {
        bool enabled = true;
        std::string channel = "stable";  // stable, beta
        int check_interval_minutes = 360;
        std::string repo_owner;
        std::string repo_name;
        bool require_sha256 = true;
        bool require_authenticode = true;
        std::vector<std::string> publisher_cert_thumbprints;
    };

    UpdateConfig getUpdateConfig() const;
    void setUpdateConfig(const UpdateConfig& config);
};
```

### 3. 目录结构

```
%LOCALAPPDATA%\ClawDeskMCP\
├── ClawDeskMCP.exe          # 主程序
├── Updater.exe              # 升级器
├── config.json              # 配置文件
├── updates\
│   ├── staging\             # 下载临时目录
│   │   ├── ClawDeskMCP-0.5.0-win-x64.exe
│   │   └── ClawDeskMCP-0.5.0-win-x64.sha256
│   ├── backup\              # 备份目录
│   │   └── 0.4.2\
│   │       └── ClawDeskMCP.exe
│   └── update.log           # 升级日志
└── logs\
    └── audit.log            # 审计日志
```

### 4. GitHub Release 规范

#### 4.1 Release 命名

```
v0.5.0 - 自动更新功能
```

#### 4.2 资产命名

```
ClawDeskMCP-0.5.0-win-x64.exe
ClawDeskMCP-0.5.0-win-x64.sha256
ClawDeskMCP-0.5.0-win-x86.exe
ClawDeskMCP-0.5.0-win-x86.sha256
```

#### 4.3 SHA256 文件格式

```
a1b2c3d4e5f6... *ClawDeskMCP-0.5.0-win-x64.exe
```

#### 4.4 Release Notes 模板

```markdown
## 新增功能

- ✅ 自动更新检查
- ✅ 版本比较

## 改进

- 优化性能

## 修复

- 修复 Bug

## 下载

- [Windows x64](https://github.com/owner/repo/releases/download/v0.5.0/ClawDeskMCP-0.5.0-win-x64.exe)
- [Windows x86](https://github.com/owner/repo/releases/download/v0.5.0/ClawDeskMCP-0.5.0-win-x86.exe)

## 校验

SHA256:

- x64: a1b2c3d4e5f6...
- x86: x9y8z7w6v5u4...
```

### 5. API 集成

#### 5.1 GitHub API 调用

```cpp
// 获取最新 Release
std::string getLatestRelease(const std::string& owner,
                              const std::string& repo) {
    std::string url = "https://api.github.com/repos/" +
                      owner + "/" + repo + "/releases/latest";

    // 使用 cpp-httplib 发送请求
    httplib::Client cli("api.github.com");
    cli.set_default_headers({
        {"User-Agent", "ClawDeskMCP/" CLAWDESK_VERSION},
        {"Accept", "application/vnd.github.v3+json"}
    });

    auto res = cli.Get(url.c_str());
    if (res && res->status == 200) {
        return res->body;
    }
    return "";
}
```

#### 5.2 解析 Release 信息

```cpp
VersionInfo parseReleaseInfo(const std::string& json) {
    auto j = nlohmann::json::parse(json);

    VersionInfo info;
    info.version = j["tag_name"].get<std::string>();
    info.release_notes_url = j["html_url"].get<std::string>();
    info.published_at = j["published_at"].get<std::string>();
    info.is_prerelease = j["prerelease"].get<bool>();

    // 查找匹配的资产
    for (const auto& asset : j["assets"]) {
        std::string name = asset["name"].get<std::string>();
        if (name.find("win-x64.exe") != std::string::npos) {
            info.download_url = asset["browser_download_url"];
        }
        if (name.find("win-x64.sha256") != std::string::npos) {
            info.sha256_url = asset["browser_download_url"];
        }
    }

    return info;
}
```

### 6. 用户界面

#### 6.1 托盘菜单扩展

```cpp
// 添加更新菜单项
HMENU hMenu = CreatePopupMenu();

// ... 现有菜单项 ...

AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
AppendMenu(hMenu, MF_STRING, ID_TRAY_CHECK_UPDATE, "检查更新...");
AppendMenu(hMenu, MF_STRING, ID_TRAY_UPDATE_SETTINGS, "更新设置...");
```

#### 6.2 更新通知对话框

```cpp
void showUpdateNotification(const VersionInfo& info) {
    std::wstring message =
        L"发现新版本: " + toWString(info.version) + L"\n\n" +
        L"当前版本: " CLAWDESK_VERSION L"\n\n" +
        L"是否立即下载并安装？";

    int result = MessageBox(NULL, message.c_str(),
                           L"更新可用",
                           MB_YESNO | MB_ICONINFORMATION);

    if (result == IDYES) {
        startUpdate(info);
    }
}
```

#### 6.3 下载进度窗口

```cpp
class UpdateProgressWindow {
public:
    void show();
    void updateProgress(double percentage, const std::string& status);
    void setStatus(const std::string& status);
    void close();

private:
    HWND hwnd_;
    HWND progressBar_;
    HWND statusLabel_;
};
```

### 7. 安全实现

#### 7.1 SHA256 校验

```cpp
#include <windows.h>
#include <wincrypt.h>

bool verifySHA256(const std::string& file_path,
                  const std::string& expected_hash) {
    // 1. 读取文件
    std::ifstream file(file_path, std::ios::binary);
    if (!file) return false;

    // 2. 计算 SHA256
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    CryptAcquireContext(&hProv, NULL, NULL,
                        PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        CryptHashData(hHash, (BYTE*)buffer, file.gcount(), 0);
    }

    // 3. 获取哈希值
    DWORD hashLen = 32;
    BYTE hash[32];
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

    // 4. 转换为十六进制字符串
    std::string calculated_hash = bytesToHex(hash, hashLen);

    // 5. 比较
    return calculated_hash == expected_hash;
}
```

#### 7.2 Authenticode 验证

```cpp
#include <wintrust.h>

bool verifyAuthenticode(const std::string& file_path) {
    WINTRUST_FILE_INFO fileInfo = {0};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = toWString(file_path).c_str();

    WINTRUST_DATA trustData = {0};
    trustData.cbStruct = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;

    GUID actionID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    LONG status = WinVerifyTrust(NULL, &actionID, &trustData);

    return status == ERROR_SUCCESS;
}
```

### 8. 审计日志

扩展 `AuditLogger` 支持更新事件：

```cpp
// 更新事件类型
enum class UpdateEvent {
    CheckStarted,
    UpdateAvailable,
    DownloadStarted,
    DownloadCompleted,
    DownloadFailed,
    VerifyStarted,
    VerifyCompleted,
    VerifyFailed,
    ApplyStarted,
    ApplyCompleted,
    ApplyFailed,
    RollbackStarted,
    RollbackCompleted
};

// 记录更新事件
void logUpdateEvent(UpdateEvent event,
                    const std::string& version,
                    const std::string& details = "") {
    nlohmann::json entry;
    entry["timestamp"] = getCurrentTimestamp();
    entry["event"] = updateEventToString(event);
    entry["version"] = version;
    entry["current_version"] = CLAWDESK_VERSION;
    if (!details.empty()) {
        entry["details"] = details;
    }

    auditLogger->log(entry);
}
```

### 9. 配置文件扩展

在 `config.json` 中添加更新配置：

```json
{
    "version": "0.4.2",
    "auth_token": "...",
    "server": {
        "port": 35182,
        "listen_address": "0.0.0.0"
    },
    "updates": {
        "enabled": true,
        "channel": "stable",
        "check_interval_minutes": 360,
        "auto_install": false,
        "repo": {
            "owner": "your-org",
            "name": "ClawDeskMCP"
        },
        "verify": {
            "require_sha256": true,
            "require_authenticode": true,
            "publisher_cert_thumbprints": ["ABCD1234567890..."]
        },
        "last_check": "2026-02-05T11:00:00Z",
        "last_update": "2026-02-05T10:00:00Z"
    }
}
```

### 10. 测试计划

#### 10.1 单元测试

- [ ] 版本比较逻辑
- [ ] SHA256 计算
- [ ] JSON 解析

#### 10.2 集成测试

- [ ] GitHub API 调用
- [ ] 下载功能
- [ ] 校验功能

#### 10.3 端到端测试

- [ ] 完整升级流程
- [ ] 回滚机制
- [ ] 错误处理

### 11. 发布流程

#### 11.1 构建脚本扩展

在 `scripts/build.sh` 中添加发布功能：

```bash
# 生成 SHA256
sha256sum build/x64/ClawDeskMCP.exe > build/x64/ClawDeskMCP-${VERSION}-win-x64.sha256
sha256sum build/x86/ClawDeskMCP.exe > build/x86/ClawDeskMCP-${VERSION}-win-x86.sha256

# 重命名文件
mv build/x64/ClawDeskMCP.exe build/x64/ClawDeskMCP-${VERSION}-win-x64.exe
mv build/x86/ClawDeskMCP.exe build/x86/ClawDeskMCP-${VERSION}-win-x86.exe
```

#### 11.2 GitHub Release 脚本

```bash
#!/bin/bash
# scripts/release.sh

VERSION=$1
CHANGELOG=$2

# 创建 GitHub Release
gh release create "v${VERSION}" \
  --title "v${VERSION}" \
  --notes-file "${CHANGELOG}" \
  build/x64/ClawDeskMCP-${VERSION}-win-x64.exe \
  build/x64/ClawDeskMCP-${VERSION}-win-x64.sha256 \
  build/x86/ClawDeskMCP-${VERSION}-win-x86.exe \
  build/x86/ClawDeskMCP-${VERSION}-win-x86.sha256
```

### 12. 风险和缓解

| 风险            | 影响           | 缓解措施             |
| --------------- | -------------- | -------------------- |
| 下载失败        | 无法更新       | 重试机制、备用下载源 |
| 校验失败        | 安全风险       | 拒绝安装、记录日志   |
| 升级失败        | 程序无法启动   | 自动回滚、保留旧版本 |
| 网络问题        | 无法检查更新   | 离线模式、手动更新   |
| GitHub API 限制 | 频繁请求被限制 | 缓存、合理的检查间隔 |

### 13. 多电脑部署考虑

对于多电脑部署场景，可以考虑：

1. **集中式更新服务器**
    - 在局域网内搭建更新服务器
    - 所有电脑从内网服务器下载更新
    - 减少外网带宽消耗

2. **分批更新**
    - 先在测试电脑上更新
    - 验证无问题后再更新其他电脑
    - 避免同时更新所有电脑

3. **更新策略配置**
    ```json
    {
        "updates": {
            "strategy": "manual", // auto, manual, scheduled
            "schedule": {
                "day_of_week": "sunday",
                "time": "02:00"
            }
        }
    }
    ```

## 总结

你的升级机制设计非常完善，涵盖了安全性、可靠性、可追溯性等关键方面。建议分阶段实施，先实现基础的版本检查和通知功能，再逐步添加自动下载、安装和回滚功能。

关键要点：

1. ✅ 安全第一：SHA256 + Authenticode
2. ✅ 可靠性：备份 + 回滚
3. ✅ 可追溯：审计日志
4. ✅ 用户友好：清晰的提示和进度
5. ✅ 灵活配置：支持多种更新策略

这个设计可以作为 v0.5.0 及后续版本的重要功能！
