# 自动升级设计（Auto Update）

> 本文档定义 WinAgent / ClawDesk MCP Server 的自动升级机制。  
> 目标是在 **安全、可验证、可回滚** 的前提下，从 GitHub 仓库获取并升级到最新可执行版本。

---

## 1. 升级目标

- 支持从指定 GitHub 仓库 **自动检查新版本**
- 支持 **自动 / 手动触发升级**
- 支持 **完整性校验 + 发行者身份校验**
- 升级失败可 **自动回滚**
- 升级过程 **可审计、可追踪**

---

## 2. 发布约定（GitHub 仓库）

### 2.1 Release 类型

- **Stable**
  - GitHub Release
  - `draft = false`
  - `prerelease = false`
- **Beta**
  - GitHub Release
  - `prerelease = true`

客户端默认只升级 Stable。

---

### 2.2 资产命名规范（强约定）

每个 Release 至少包含以下资产之一：

- `WinAgent-<version>-win-x64.exe`
- 或 `WinAgent-<version>-win-x64.zip`

同时必须包含：

- `WinAgent-<version>-win-x64.sha256`

（推荐）：

- Authenticode 代码签名（exe 内嵌）

---

## 3. 版本检查机制

### 3.1 API 使用

- 最新稳定版：

```
GET https://api.github.com/repos/{owner}/{repo}/releases/latest
```

### 3.2 检查频率

- 默认：每 6 小时
- 托盘菜单支持“立即检查更新”

### 3.3 版本比较

- 使用语义化版本（SemVer）
- 若 `latest.version > current.version` → 可升级

---

## 4. 升级流程（客户端）

### 4.1 状态机

```
Idle
└─ Checking
└─ UpdateAvailable
└─ Downloading
└─ Verifying
└─ Applying
└─ Restarting
└─ Idle
```

失败状态：

- DownloadFailed
- VerifyFailed
- ApplyFailed
- Rollback

---

### 4.2 详细步骤

1. 检查最新 Release
2. 选择匹配平台的资产
3. 下载到 staging 目录：
   %LOCALAPPDATA%\WinAgent\updates\staging\

4. 校验：

- SHA256 校验
- Authenticode 签名验证

1. 写入升级器（Updater）
2. 主程序退出
3. 升级器：

- 备份旧版本
- 替换新版本
- 启动新版本

1. 新版本启动后写入：
   last_update_ok = true

---

## 5. 安全校验（必须）

### 5.1 完整性校验

- 使用 `.sha256` 文件
- 客户端本地重新计算哈希
- 不一致 → 直接失败

### 5.2 发行者身份校验（强烈推荐）

- 使用 Windows Authenticode
- 调用 `WinVerifyTrust`
- 校验证书指纹（pin）

不通过 → 直接失败

---

## 6. 回滚机制

- 升级前备份旧版本：
  %LOCALAPPDATA%\WinAgent\updates\backup<version>\

- 若新版本在 N 秒内未写入 `last_update_ok`
- 自动回滚
- 重启旧版本
- MVP 阶段仅保留最近 1 个备份版本

---

## 7. 配置项（config.json）

```json
{
    "updates": {
        "enabled": true,
        "channel": "stable",
        "check_interval_minutes": 360,
        "repo": {
            "owner": "YOUR_ORG",
            "name": "YOUR_REPO"
        },
        "asset_pattern": "win-x64",
        "verify": {
            "require_sha256": true,
            "require_authenticode": true,
            "publisher_cert_thumbprints": ["ABCD1234..."]
        }
    }
}
```

1. 托盘交互
   • 检查更新…
   • 自动更新：开启 / 关闭
   • 更新通道：Stable / Beta
   • 查看更新日志（打开 Release 页面）

2. 审计日志

升级相关事件必须写入审计日志：
• update_check_started
• update_available
• update_download_finished
• update_verify_failed
• update_apply_finished
• update_rollback

1. 实施里程碑

R4（Auto Update）
• GitHub Release API 接入
• SHA256 校验
• Authenticode 校验
• 外置升级器
• 回滚机制
