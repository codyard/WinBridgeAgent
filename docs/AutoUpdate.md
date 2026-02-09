# 自动更新功能文档

## 概述

ClawDesk MCP Server 从 v0.5.0 开始支持自动更新功能，可以自动检查 GitHub 上的新版本并提示用户下载更新。

## 功能特性

### Phase 1：基础版本检查（v0.5.0）

- ✅ **GitHub API 集成**：自动从 GitHub Releases 获取最新版本信息
- ✅ **语义化版本比较**：智能比较版本号，支持预发布版本（beta、alpha）
- ✅ **托盘菜单集成**：右键托盘图标即可检查更新
- ✅ **更新通知**：发现新版本时显示友好的通知对话框
- ✅ **一键下载**：自动打开 GitHub Release 页面下载新版本
- ✅ **多语言支持**：支持简体中文、英语等多种语言
- ✅ **配置灵活**：支持 stable/beta 通道切换

## 使用方法

### 手动检查更新

1. 右键点击系统托盘中的 ClawDesk MCP Server 图标
2. 选择"检查更新..."（或 "Check for Updates..."）
3. 等待检查完成
4. 如果有新版本，会显示版本信息和 Release Notes
5. 点击"是"打开 GitHub Release 页面下载

### 配置更新设置

编辑 `config.json` 文件中的 `update` 节：

```json
{
  "update": {
    "enabled": true,
    "check_interval_hours": 6,
    "channel": "stable",
    "github_repo": "ihugang/ClawDeskMCP",
    "verify_signature": true,
    "last_check": "",
    "skipped_version": ""
  }
}
```

#### 配置项说明

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `enabled` | boolean | `true` | 是否启用自动更新检查 |
| `check_interval_hours` | integer | `6` | 自动检查更新的间隔（小时） |
| `channel` | string | `"stable"` | 更新通道：`"stable"` 或 `"beta"` |
| `github_repo` | string | `"ihugang/ClawDeskMCP"` | GitHub 仓库（格式：owner/repo） |
| `verify_signature` | boolean | `true` | 是否验证代码签名（Phase 4 实现） |
| `last_check` | string | `""` | 上次检查时间（ISO 8601 格式，自动更新） |
| `skipped_version` | string | `""` | 跳过的版本号（暂未使用） |

### 切换更新通道

**Stable 通道（稳定版）**：
```json
{
  "update": {
    "channel": "stable"
  }
}
```

**Beta 通道（测试版）**：
```json
{
  "update": {
    "channel": "beta"
  }
}
```

切换通道后，下次检查更新时会包含预发布版本。

### 禁用自动更新

如果不想接收更新通知，可以禁用自动更新：

```json
{
  "update": {
    "enabled": false
  }
}
```

## 技术实现

### 架构设计

```
┌─────────────────┐
│  TrayManager    │  托盘菜单
│  (main.cpp)     │
└────────┬────────┘
         │ 用户点击"检查更新"
         ▼
┌─────────────────┐
│ UpdateChecker   │  更新检查器
│                 │
├─────────────────┤
│ • GitHub API    │  调用 GitHub REST API
│ • Version比较   │  语义化版本解析和比较
│ • 资产选择      │  根据平台选择安装包
│ • 重试机制      │  网络错误自动重试
└────────┬────────┘
         │ 异步回调
         ▼
┌─────────────────┐
│ 更新通知对话框   │  显示版本信息
│ (MessageBox)    │  用户确认下载
└─────────────────┘
         │ 用户点击"是"
         ▼
┌─────────────────┐
│ ShellExecute    │  打开浏览器
│ GitHub Release  │  下载新版本
└─────────────────┘
```

### UpdateChecker 类

**核心方法**：

```cpp
// 同步检查更新
UpdateCheckResult checkForUpdates(bool includePrereleases = false);

// 异步检查更新
void checkForUpdatesAsync(
    std::function<void(const UpdateCheckResult&)> callback,
    bool includePrereleases = false
);
```

**Version 结构**：

```cpp
struct Version {
    int major;
    int minor;
    int patch;
    std::string prerelease;  // 如 "beta.1"
    
    static Version parse(const std::string& versionStr);
    bool operator<(const Version& other) const;
    bool operator>(const Version& other) const;
};
```

**版本比较规则**：

1. 比较 major.minor.patch（数字大小）
2. 预发布版本 < 正式版本（如 `1.0.0-beta.1` < `1.0.0`）
3. 预发布版本之间按字符串比较（如 `beta.1` < `beta.2`）

### GitHub API 调用

**端点**：
- Stable: `GET https://api.github.com/repos/{owner}/{repo}/releases/latest`
- Beta: `GET https://api.github.com/repos/{owner}/{repo}/releases`

**请求头**：
```
User-Agent: ClawDeskMCP-UpdateChecker/1.0
Accept: application/vnd.github.v3+json
```

**响应示例**：
```json
{
  "tag_name": "v0.5.0",
  "name": "v0.5.0 - Auto-Update Phase 1",
  "body": "## 新增功能\n- 自动更新检查\n...",
  "html_url": "https://github.com/ihugang/ClawDeskMCP/releases/tag/v0.5.0",
  "prerelease": false,
  "published_at": "2026-02-05T03:30:00Z",
  "assets": [
    {
      "name": "ClawDeskMCP-0.5.0-win-x64.exe",
      "browser_download_url": "https://github.com/.../ClawDeskMCP-0.5.0-win-x64.exe",
      "size": 5242880
    },
    {
      "name": "ClawDeskMCP-0.5.0-win-x64.exe.sha256",
      "browser_download_url": "https://github.com/.../ClawDeskMCP-0.5.0-win-x64.exe.sha256",
      "size": 64
    }
  ]
}
```

### 资产选择逻辑

UpdateChecker 会根据当前平台自动选择匹配的安装包：

| 平台 | 文件名模式 | SHA256 文件 |
|------|-----------|------------|
| x64 | `ClawDeskMCP-*-win-x64.exe` | `*.exe.sha256` |
| x86 | `ClawDeskMCP-*-win-x86.exe` | `*.exe.sha256` |
| ARM64 | `ClawDeskMCP-*-win-arm64.exe` | `*.exe.sha256` |

### 重试机制

网络请求失败时自动重试，使用指数退避策略：

- 第 1 次重试：等待 1 秒
- 第 2 次重试：等待 2 秒
- 第 3 次重试：等待 4 秒
- 最多重试 3 次

## 用户界面

### 托盘菜单

```
┌─────────────────────────────┐
│ ClawDesk MCP Server         │
├─────────────────────────────┤
│ Status: Running             │
│ Port: 35182                 │
│ Listen: All interfaces      │
├─────────────────────────────┤
│ Usage Statistics            │
│ View Logs                   │
│ Dashboard                   │
├─────────────────────────────┤
│ ✨ Check for Updates...     │  ← 新增
├─────────────────────────────┤
│ Settings                    │
│ Toggle Listen Address       │
│ Open Config                 │
│ About                       │
├─────────────────────────────┤
│ Exit                        │
└─────────────────────────────┘
```

### 更新通知对话框

**有新版本时**：
```
┌──────────────────────────────────────┐
│ Check for Updates                    │
├──────────────────────────────────────┤
│ A new version is available!          │
│                                      │
│ Current: 0.4.2                       │
│ Latest: 0.5.0                        │
│                                      │
│ v0.5.0 - Auto-Update Phase 1         │
│                                      │
│ Visit GitHub to download?            │
├──────────────────────────────────────┤
│              [ Yes ]  [ No ]         │
└──────────────────────────────────────┘
```

**已是最新版本时**：
```
┌──────────────────────────────────────┐
│ Check for Updates                    │
├──────────────────────────────────────┤
│ You are running the latest version:  │
│ 0.5.0                                │
├──────────────────────────────────────┤
│              [ OK ]                  │
└──────────────────────────────────────┘
```

**检查失败时**：
```
┌──────────────────────────────────────┐
│ Check for Updates                    │
├──────────────────────────────────────┤
│ Failed to check for updates:         │
│ Network error                        │
├──────────────────────────────────────┤
│              [ OK ]                  │
└──────────────────────────────────────┘
```

## 多语言支持

### 翻译键

| 键 | 英文 | 简体中文 |
|----|------|----------|
| `tray.check_update` | Check for Updates... | 检查更新... |
| `update.title` | Check for Updates | 检查更新 |
| `update.already_checking` | Already checking for updates... | 正在检查更新... |
| `update.check_failed` | Failed to check for updates:  | 检查更新失败： |
| `update.up_to_date` | You are running the latest version:  | 您正在使用最新版本： |
| `update.available` | A new version is available!\n\n | 发现新版本！\n\n |
| `update.visit_github` | Visit GitHub to download? | 访问 GitHub 下载？ |

## 安全性

### Phase 1（当前版本）

- ✅ HTTPS 连接（GitHub API）
- ✅ 版本号验证（防止降级攻击）
- ✅ 用户确认（不自动下载）

### Phase 2-4（未来版本）

- ⏳ SHA256 哈希验证
- ⏳ Authenticode 代码签名验证
- ⏳ 证书指纹固定

## 故障排除

### 检查更新失败

**问题**：点击"检查更新"后显示失败

**可能原因**：
1. 网络连接问题
2. GitHub API 速率限制（未认证：60次/小时）
3. 防火墙阻止 HTTPS 连接

**解决方法**：
1. 检查网络连接
2. 等待一段时间后重试
3. 检查防火墙设置，允许程序访问 `api.github.com`

### 找不到匹配的安装包

**问题**：检查到新版本，但提示"No matching asset found"

**可能原因**：
1. Release 中没有当前平台的安装包
2. 文件命名不符合规范

**解决方法**：
1. 手动访问 GitHub Release 页面下载
2. 联系开发者报告问题

### 版本号显示异常

**问题**：显示的版本号不正确

**可能原因**：
1. `CMakeLists.txt` 中的版本号未更新
2. 编译时未定义 `CLAWDESK_VERSION`

**解决方法**：
1. 检查 `CMakeLists.txt` 中的 `project(ClawDeskMCP VERSION x.x.x)`
2. 重新编译程序

## 开发指南

### 添加新的更新通道

1. 修改 `config.json` 的 `update.channel` 字段
2. 在 `UpdateChecker::checkForUpdates()` 中添加通道逻辑
3. 更新文档

### 自定义 GitHub 仓库

如果要从其他仓库检查更新：

```json
{
  "update": {
    "github_repo": "your-username/your-repo"
  }
}
```

### 发布新版本

1. 更新 `CMakeLists.txt` 中的版本号
2. 更新 `CHANGELOG.md`
3. 编译所有平台的可执行文件（x64, x86, ARM64）
4. 生成 SHA256 哈希文件
5. 创建 GitHub Release
6. 上传所有文件

**文件命名规范**：
```
ClawDeskMCP-{version}-win-{arch}.exe
ClawDeskMCP-{version}-win-{arch}.exe.sha256
```

示例：
```
ClawDeskMCP-0.5.0-win-x64.exe
ClawDeskMCP-0.5.0-win-x64.exe.sha256
ClawDeskMCP-0.5.0-win-x86.exe
ClawDeskMCP-0.5.0-win-x86.exe.sha256
```

## 未来计划

### Phase 2：下载和验证（v0.6.0）

- ⏳ 下载管理器（支持断点续传）
- ⏳ SHA256 哈希验证
- ⏳ 下载进度显示
- ⏳ 磁盘空间检查

### Phase 3：自动升级（v0.7.0）

- ⏳ 外置升级器（Updater.exe）
- ⏳ 自动备份和恢复
- ⏳ 静默安装
- ⏳ 自动重启

### Phase 4：高级功能（v0.8.0）

- ⏳ Authenticode 签名验证
- ⏳ 证书指纹固定
- ⏳ 自动回滚机制
- ⏳ 增量更新

## 参考资料

- [GitHub REST API 文档](https://docs.github.com/en/rest)
- [语义化版本规范](https://semver.org/)
- [.kiro/specs/auto-update/](../.kiro/specs/auto-update/)
  - `requirements.md` - 需求文档
  - `design.md` - 设计文档
  - `tasks.md` - 任务清单

## 更新日志

### v0.5.0 (2026-02-05)

- ✅ 实现 Phase 1：基础版本检查
- ✅ UpdateChecker 类
- ✅ ConfigManager 扩展
- ✅ 托盘菜单集成
- ✅ 多语言支持
