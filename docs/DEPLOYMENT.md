# ClawDesk MCP Server 部署说明文档

## 目录

1. [概述](#概述)
2. [开发环境准备](#开发环境准备)
3. [编译构建](#编译构建)
4. [版本发布](#版本发布)
5. [部署安装](#部署安装)
6. [配置管理](#配置管理)
7. [故障排除](#故障排除)
8. [附录](#附录)

---

## 概述

本文档详细说明 ClawDesk MCP Server 的完整部署流程，包括：

- **开发环境**：macOS 交叉编译到 Windows
- **目标平台**：Windows 10/11 (x64, x86, ARM64)
- **编译工具**：MinGW-w64, CMake
- **部署方式**：单文件可执行程序 + 配置文件

### 版本信息

- **当前版本**：v0.5.0
- **发布日期**：2026-02-05
- **主要更新**：自动更新功能 Phase 1

---

## 开发环境准备

### 1. macOS 开发环境

#### 1.1 安装 Xcode Command Line Tools

```bash
# 安装 Xcode Command Line Tools
xcode-select --install

# 同意许可证
sudo xcodebuild -license accept
```

#### 1.2 安装 Homebrew

```bash
# 安装 Homebrew（如果未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

#### 1.3 安装 MinGW-w64 工具链

```bash
# 安装 MinGW-w64（用于交叉编译到 Windows）
brew install mingw-w64

# 验证安装
x86_64-w64-mingw32-gcc --version
i686-w64-mingw32-gcc --version
```

#### 1.4 安装 CMake

```bash
# 安装 CMake
brew install cmake

# 验证安装
cmake --version  # 应该 >= 3.20
```

### 2. Windows 开发环境（可选）

如果在 Windows 上直接编译：

#### 2.1 安装 MinGW-w64

1. 下载 [MinGW-w64](https://www.mingw-w64.org/downloads/)
2. 安装到 `C:\mingw64`
3. 添加到系统 PATH：`C:\mingw64\bin`

#### 2.2 安装 CMake

1. 下载 [CMake](https://cmake.org/download/)
2. 安装并添加到 PATH

### 3. 依赖库（已包含）

项目已包含所有必要的第三方库：

```
third_party/
├── cpp-httplib/        # HTTP 客户端库
├── nlohmann-json/      # JSON 解析库
└── stb/               # 图像处理库
```

无需额外安装依赖。

---

## 编译构建

### 方法 1：使用自动化脚本（推荐）

#### 完整构建（所有平台）

```bash
cd /path/to/WinAgent
./scripts/build.sh
```

此脚本会：
1. 自动递增版本号（patch 版本 +1）
2. 编译 x64 版本
3. 编译 x86 版本
4. 编译 ARM64 版本（如果工具链可用）
5. 部署到指定目录

#### 构建输出

```
build/
├── x64/
│   └── ClawDeskMCP.exe          # x64 版本
├── x86/
│   └── ClawDeskMCP.exe          # x86 版本
└── arm64/
    └── ClawDeskMCP.exe          # ARM64 版本
```

### 方法 2：手动构建

#### 2.1 构建 x64 版本

```bash
# 创建构建目录
mkdir -p build/x64
cd build/x64

# 配置 CMake
cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x64.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(sysctl -n hw.ncpu)

# 输出：ClawDeskMCP.exe
```

#### 2.2 构建 x86 版本

```bash
# 创建构建目录
mkdir -p build/x86
cd build/x86

# 配置 CMake
cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x86.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(sysctl -n hw.ncpu)

# 输出：ClawDeskMCP.exe
```

#### 2.3 构建 ARM64 版本

```bash
# 创建构建目录
mkdir -p build/arm64
cd build/arm64

# 配置 CMake
cmake ../.. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-arm64.cmake \
    -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(sysctl -n hw.ncpu)

# 输出：ClawDeskMCP.exe
```

### 3. 编译选项说明

#### CMake 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CMAKE_BUILD_TYPE` | 构建类型（Release/Debug） | Release |
| `CMAKE_TOOLCHAIN_FILE` | 工具链文件 | - |

#### Release 优化

Release 模式包含以下优化：

- 静态链接 MinGW 运行时（无需额外 DLL）
- 代码优化（-O3）
- 符号剥离（减小文件大小）
- 无控制台窗口（GUI 应用）

### 4. 验证构建

```bash
# 检查文件大小（应该在 3-6 MB）
ls -lh build/x64/ClawDeskMCP.exe

# 检查依赖（应该只依赖 Windows 系统库）
x86_64-w64-mingw32-objdump -p build/x64/ClawDeskMCP.exe | grep "DLL Name"
```

预期输出：
```
DLL Name: KERNEL32.dll
DLL Name: msvcrt.dll
DLL Name: USER32.dll
DLL Name: GDI32.dll
DLL Name: ADVAPI32.dll
DLL Name: SHELL32.dll
DLL Name: WS2_32.dll
```

---

## 版本发布

### 1. 版本号管理

#### 1.1 版本号格式

遵循语义化版本规范（SemVer）：

```
MAJOR.MINOR.PATCH[-PRERELEASE]

示例：
- 0.5.0          # 正式版本
- 0.5.0-beta.1   # 预发布版本
- 0.5.0-rc.1     # 候选版本
```

#### 1.2 更新版本号

编辑 `CMakeLists.txt`：

```cmake
project(ClawDeskMCP VERSION 0.5.0 LANGUAGES CXX)
```

或使用自动化脚本（自动递增 patch 版本）：

```bash
./scripts/build.sh  # 会自动从 0.5.0 -> 0.5.1
```

### 2. 生成发布文件

#### 2.1 编译所有平台版本

```bash
./scripts/build.sh
```

#### 2.2 重命名文件

```bash
cd build

# x64 版本
cp x64/ClawDeskMCP.exe ClawDeskMCP-0.5.0-win-x64.exe

# x86 版本
cp x86/ClawDeskMCP.exe ClawDeskMCP-0.5.0-win-x86.exe

# ARM64 版本（如果有）
cp arm64/ClawDeskMCP.exe ClawDeskMCP-0.5.0-win-arm64.exe
```

#### 2.3 生成 SHA256 哈希

```bash
# macOS
shasum -a 256 ClawDeskMCP-0.5.0-win-x64.exe > ClawDeskMCP-0.5.0-win-x64.exe.sha256
shasum -a 256 ClawDeskMCP-0.5.0-win-x86.exe > ClawDeskMCP-0.5.0-win-x86.exe.sha256
shasum -a 256 ClawDeskMCP-0.5.0-win-arm64.exe > ClawDeskMCP-0.5.0-win-arm64.exe.sha256

# Windows
certutil -hashfile ClawDeskMCP-0.5.0-win-x64.exe SHA256 > ClawDeskMCP-0.5.0-win-x64.exe.sha256
```

#### 2.4 代码签名（可选，推荐）

```bash
# 使用 Authenticode 签名（需要代码签名证书）
signtool sign /f certificate.pfx /p password /t http://timestamp.digicert.com ClawDeskMCP-0.5.0-win-x64.exe
```

### 3. 创建 GitHub Release

#### 3.1 准备 Release Notes

编辑 `CHANGELOG.md`，确保包含最新版本的更新日志。

#### 3.2 创建 Git Tag

```bash
# 创建标签
git tag -a v0.5.0 -m "Release v0.5.0 - Auto-Update Phase 1"

# 推送标签
git push origin v0.5.0
```

#### 3.3 创建 GitHub Release

1. 访问 GitHub 仓库
2. 点击 "Releases" → "Draft a new release"
3. 填写信息：
   - **Tag version**: `v0.5.0`
   - **Release title**: `v0.5.0 - Auto-Update Phase 1`
   - **Description**: 从 `CHANGELOG.md` 复制更新日志
4. 上传文件：
   - `ClawDeskMCP-0.5.0-win-x64.exe`
   - `ClawDeskMCP-0.5.0-win-x64.exe.sha256`
   - `ClawDeskMCP-0.5.0-win-x86.exe`
   - `ClawDeskMCP-0.5.0-win-x86.exe.sha256`
   - `ClawDeskMCP-0.5.0-win-arm64.exe`
   - `ClawDeskMCP-0.5.0-win-arm64.exe.sha256`
5. 选择发布类型：
   - 正式版本：取消勾选 "This is a pre-release"
   - 预发布版本：勾选 "This is a pre-release"
6. 点击 "Publish release"

### 4. 发布检查清单

- [ ] 版本号已更新（`CMakeLists.txt`）
- [ ] 更新日志已完善（`CHANGELOG.md`）
- [ ] 所有平台版本已编译
- [ ] SHA256 哈希已生成
- [ ] 文件命名符合规范
- [ ] Git 标签已创建并推送
- [ ] GitHub Release 已创建
- [ ] 所有文件已上传
- [ ] Release Notes 已填写
- [ ] 发布类型已正确设置

---

## 部署安装

### 1. 单机部署

#### 1.1 下载程序

从 GitHub Release 页面下载对应平台的版本：

```
https://github.com/ihugang/ClawDeskMCP/releases/latest
```

选择文件：
- **x64 系统**：`ClawDeskMCP-0.5.0-win-x64.exe`
- **x86 系统**：`ClawDeskMCP-0.5.0-win-x86.exe`
- **ARM64 系统**：`ClawDeskMCP-0.5.0-win-arm64.exe`

#### 1.2 验证文件完整性（推荐）

```powershell
# 下载 SHA256 文件
# 验证哈希值
certutil -hashfile ClawDeskMCP-0.5.0-win-x64.exe SHA256

# 对比 .sha256 文件中的值
```

#### 1.3 安装程序

```powershell
# 1. 创建安装目录
New-Item -Path "$env:LOCALAPPDATA\ClawDeskMCP" -ItemType Directory -Force

# 2. 复制程序文件
Copy-Item ClawDeskMCP-0.5.0-win-x64.exe "$env:LOCALAPPDATA\ClawDeskMCP\ClawDeskMCP.exe"

# 3. 首次运行（会自动生成配置文件）
& "$env:LOCALAPPDATA\ClawDeskMCP\ClawDeskMCP.exe"
```

#### 1.4 目录结构

首次运行后会生成以下目录结构：

```
%LOCALAPPDATA%\ClawDeskMCP\
├── ClawDeskMCP.exe           # 主程序
├── config.json               # 配置文件（自动生成）
├── logs\
│   └── audit.log            # 审计日志
└── updates\                 # 更新目录（v0.5.0+）
    ├── staging\             # 下载暂存
    └── backup\              # 备份目录
```

### 2. 多电脑部署

#### 2.1 使用部署脚本

编辑 `scripts/build.sh` 中的部署目标：

```bash
DEPLOY_TARGETS=(
    "Test|/Volumes/Test|192.168.31.3|35182"
    "Office|/Volumes/Office|192.168.31.4|35182"
    "Home|/Volumes/Home|192.168.31.5|35182"
)
```

运行部署：

```bash
./scripts/build.sh
```

#### 2.2 手动部署到多台电脑

**方法 1：网络共享**

```powershell
# 在每台电脑上执行
$source = "\\server\share\ClawDeskMCP.exe"
$dest = "$env:LOCALAPPDATA\ClawDeskMCP\ClawDeskMCP.exe"
Copy-Item $source $dest -Force
```

**方法 2：远程部署**

```powershell
# 使用 PowerShell Remoting
$computers = @("PC1", "PC2", "PC3")
$source = "C:\Deploy\ClawDeskMCP.exe"

foreach ($computer in $computers) {
    $session = New-PSSession -ComputerName $computer
    Copy-Item $source -Destination "$env:LOCALAPPDATA\ClawDeskMCP\" -ToSession $session
    Remove-PSSession $session
}
```

### 3. 配置 MCP 客户端

#### 3.1 获取 Bearer Token

首次运行后，查看 `config.json` 获取 Bearer Token：

```json
{
  "auth_token": "abc123def456...",
  "server_port": 35182,
  ...
}
```

或通过托盘菜单：
1. 右键托盘图标
2. 选择 "Settings"
3. 切换到 "API Key" 标签
4. 点击 "Copy" 复制 Token

#### 3.2 配置 Claude Desktop

编辑 `claude_desktop_config.json`：

```json
{
  "mcpServers": {
    "clawdesk": {
      "url": "http://192.168.31.3:35182",
      "headers": {
        "Authorization": "Bearer YOUR_TOKEN_HERE"
      },
      "description": "ClawDesk MCP Server"
    }
  }
}
```

#### 3.3 配置多电脑

```json
{
  "mcpServers": {
    "clawdesk-test": {
      "url": "http://192.168.31.3:35182",
      "headers": {
        "Authorization": "Bearer TOKEN_FROM_TEST_PC"
      },
      "description": "Test 电脑"
    },
    "clawdesk-office": {
      "url": "http://192.168.31.4:35182",
      "headers": {
        "Authorization": "Bearer TOKEN_FROM_OFFICE_PC"
      },
      "description": "Office 电脑"
    }
  }
}
```

### 4. 开机自启动

#### 4.1 通过设置窗口（推荐）

1. 右键托盘图标
2. 选择 "Settings"
3. 切换到 "Startup" 标签
4. 勾选 "Start with Windows"
5. 点击 "OK"

#### 4.2 手动配置注册表

```powershell
# 添加自启动项
$path = "$env:LOCALAPPDATA\ClawDeskMCP\ClawDeskMCP.exe"
$name = "ClawDeskMCP"
$regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"

Set-ItemProperty -Path $regPath -Name $name -Value $path
```

#### 4.3 验证自启动

```powershell
# 查看注册表
Get-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "ClawDeskMCP"
```

---

## 配置管理

### 1. 配置文件说明

配置文件位置：`%LOCALAPPDATA%\ClawDeskMCP\config.json`

### 2. 完整配置示例

```json
{
  "auth_token": "abc123def456...",
  "server_port": 35182,
  "auto_port": true,
  "listen_address": "0.0.0.0",
  "license_key": "",
  "language": "zh-CN",
  "auto_startup": true,
  "api_key": "xyz789...",
  
  "server": {
    "port": 35182,
    "auto_port": true,
    "listen_address": "0.0.0.0"
  },
  
  "security": {
    "bearer_token": "abc123def456...",
    "allowed_dirs": [
      "C:/Users",
      "C:/Temp"
    ],
    "allowed_apps": {
      "notepad": "C:/Windows/System32/notepad.exe",
      "calc": "C:/Windows/System32/calc.exe"
    },
    "allowed_commands": [
      "npm",
      "git",
      "python"
    ],
    "high_risk_confirmations": true
  },
  
  "appearance": {
    "dashboard_auto_show": true,
    "dashboard_always_on_top": false,
    "tray_icon_style": "normal",
    "log_retention_days": 30
  },
  
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

### 3. 配置项说明

#### 3.1 服务器配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `server_port` | integer | `35182` | 服务器端口 |
| `auto_port` | boolean | `true` | 端口被占用时自动选择 |
| `listen_address` | string | `"0.0.0.0"` | 监听地址 |

#### 3.2 安全配置

| 配置项 | 类型 | 说明 |
|--------|------|------|
| `bearer_token` | string | API 认证令牌 |
| `allowed_dirs` | array | 允许访问的目录白名单 |
| `allowed_apps` | object | 允许启动的应用程序 |
| `allowed_commands` | array | 允许执行的命令 |
| `high_risk_confirmations` | boolean | 高风险操作确认 |

#### 3.3 更新配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `enabled` | boolean | `true` | 启用自动更新 |
| `check_interval_hours` | integer | `6` | 检查间隔（小时） |
| `channel` | string | `"stable"` | 更新通道 |
| `github_repo` | string | `"ihugang/ClawDeskMCP"` | GitHub 仓库 |

### 4. 配置模板

提供配置模板：`resources/config.template.json`

```bash
# 使用模板创建配置
cp resources/config.template.json config.json
```

---

## 故障排除

### 1. 编译问题

#### 问题：找不到 MinGW 编译器

```
CMake Error: CMAKE_C_COMPILER not set
```

**解决方案**：

```bash
# 检查 MinGW 安装
which x86_64-w64-mingw32-gcc

# 如果未安装
brew install mingw-w64
```

#### 问题：Xcode 许可证未同意

```
You have not agreed to the Xcode license agreements
```

**解决方案**：

```bash
sudo xcodebuild -license accept
```

#### 问题：找不到 OpenSSL

```
Could not find OpenSSL
```

**解决方案**：

MinGW-w64 已包含 OpenSSL，确保使用正确的工具链文件。

### 2. 运行问题

#### 问题：程序无法启动

**检查步骤**：

1. 确认 Windows 版本（需要 Windows 10/11）
2. 检查是否有杀毒软件拦截
3. 查看事件查看器中的错误日志

#### 问题：端口被占用

```
Failed to bind to port 35182
```

**解决方案**：

1. 修改 `config.json` 中的 `server_port`
2. 或启用 `auto_port` 自动选择端口

#### 问题：无法访问 API

**检查步骤**：

1. 确认防火墙规则
2. 检查 Bearer Token 是否正确
3. 验证监听地址配置

```powershell
# 测试连接
curl http://localhost:35182/health -H "Authorization: Bearer YOUR_TOKEN"
```

### 3. 更新问题

#### 问题：检查更新失败

**可能原因**：
- 网络连接问题
- GitHub API 速率限制
- 防火墙阻止 HTTPS

**解决方案**：

1. 检查网络连接
2. 等待一段时间后重试
3. 配置防火墙允许访问 `api.github.com`

### 4. 日志查看

#### 审计日志

位置：`%LOCALAPPDATA%\ClawDeskMCP\logs\audit.log`

```powershell
# 查看最近的日志
Get-Content "$env:LOCALAPPDATA\ClawDeskMCP\logs\audit.log" -Tail 50
```

#### 本地化日志

位置：`%LOCALAPPDATA%\ClawDeskMCP\logs\localization.log`

---

## 附录

### A. 文件命名规范

#### 可执行文件

```
ClawDeskMCP-{version}-win-{arch}.exe

示例：
- ClawDeskMCP-0.5.0-win-x64.exe
- ClawDeskMCP-0.5.0-win-x86.exe
- ClawDeskMCP-0.5.0-win-arm64.exe
```

#### SHA256 文件

```
ClawDeskMCP-{version}-win-{arch}.exe.sha256

示例：
- ClawDeskMCP-0.5.0-win-x64.exe.sha256
```

### B. 端口说明

| 端口 | 协议 | 用途 |
|------|------|------|
| 35182 | HTTP | 默认 API 端口 |
| 自动 | HTTP | auto_port 启用时 |

### C. 系统要求

#### 最低要求

- **操作系统**：Windows 10 (1809+) / Windows 11
- **处理器**：x64, x86, 或 ARM64
- **内存**：512 MB RAM
- **磁盘空间**：50 MB
- **网络**：可选（用于远程访问和更新）

#### 推荐配置

- **操作系统**：Windows 11
- **处理器**：x64 双核或更高
- **内存**：2 GB RAM
- **磁盘空间**：200 MB
- **网络**：千兆以太网或 Wi-Fi

### D. 相关文档

- [README.md](../README.md) - 项目概述
- [CHANGELOG.md](../CHANGELOG.md) - 更新日志
- [AutoUpdate.md](AutoUpdate.md) - 自动更新功能
- [MULTI_COMPUTER_SETUP.md](MULTI_COMPUTER_SETUP.md) - 多电脑配置

### E. 技术支持

- **GitHub Issues**: https://github.com/ihugang/ClawDeskMCP/issues
- **文档**: https://github.com/ihugang/ClawDeskMCP/tree/main/docs
- **Email**: support@example.com

### F. 许可证

本项目采用 [许可证名称] 许可证。详见 [LICENSE](../LICENSE) 文件。

---

## 快速参考

### 编译命令

```bash
# 完整构建
./scripts/build.sh

# 手动构建 x64
mkdir -p build/x64 && cd build/x64
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x64.cmake -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### 部署命令

```powershell
# 安装
New-Item -Path "$env:LOCALAPPDATA\ClawDeskMCP" -ItemType Directory -Force
Copy-Item ClawDeskMCP.exe "$env:LOCALAPPDATA\ClawDeskMCP\"

# 运行
& "$env:LOCALAPPDATA\ClawDeskMCP\ClawDeskMCP.exe"

# 配置自启动
$path = "$env:LOCALAPPDATA\ClawDeskMCP\ClawDeskMCP.exe"
Set-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "ClawDeskMCP" -Value $path
```

### 验证命令

```powershell
# 验证 SHA256
certutil -hashfile ClawDeskMCP.exe SHA256

# 测试 API
curl http://localhost:35182/health -H "Authorization: Bearer YOUR_TOKEN"

# 查看日志
Get-Content "$env:LOCALAPPDATA\ClawDeskMCP\logs\audit.log" -Tail 50
```

---

**文档版本**: v1.0  
**最后更新**: 2026-02-05  
**适用版本**: ClawDesk MCP Server v0.5.0+
