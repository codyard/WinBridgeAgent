# ClawDesk MCP Server - 快速开始指南

## 仓库配置

你的项目使用双仓库策略：

- **私有仓库**（源代码）：https://github.com/ihugang/ClawWindowsDeskMcp_SourceCode.git
- **公开仓库**（发布版本）：https://github.com/ihugang/ClawWindowsDeskMcp.git

配置已保存在 `release.config` 文件中。

## 第一次设置

### 方式 1：使用自动化脚本（推荐）

```bash
# 运行初始化脚本，它会引导你完成所有设置
../scripts/init-repos.sh
```

这个脚本会：
1. ✅ 初始化私有仓库（当前目录）
2. ✅ 配置远程仓库地址
3. ✅ 构建程序
4. ✅ 生成发布文件
5. ✅ 初始化公开仓库
6. ✅ 推送到远程

### 方式 2：手动设置

#### 步骤 1: 初始化私有仓库

```bash
# 初始化 git 仓库
git init

# 添加所有文件
git add .

# 创建初始提交
git commit -m "Initial commit: ClawDesk MCP Server source code"

# 配置远程仓库
git remote add origin https://github.com/ihugang/ClawWindowsDeskMcp_SourceCode.git

# 推送到远程
git branch -M main
git push -u origin main
```

#### 步骤 2: 构建程序

```bash
../scripts/build.sh
```

#### 步骤 3: 生成发布文件

```bash
../scripts/release.sh
```

#### 步骤 4: 初始化公开仓库

```bash
# 进入发布目录
cd ../ClawDeskMCP-Release

# 初始化 git 仓库
git init

# 添加所有文件
git add .

# 创建初始提交
git commit -m "Initial release: ClawDesk MCP Server v0.2.0"

# 创建版本标签
git tag v0.2.0

# 配置远程仓库
git remote add origin https://github.com/ihugang/ClawWindowsDeskMcp.git

# 推送到远程
git branch -M main
git push -u origin main --tags

# 返回源代码目录
cd -
```

## 日常发布流程

### 发布新版本（一键完成）

```bash
# 发布 v0.3.0 版本
../scripts/auto-release.sh v0.3.0
```

这个命令会自动：
1. ✅ 更新版本号（../scripts/release.sh 和 CMakeLists.txt）
2. ✅ 构建所有架构（x64, x86, ARM64）
3. ✅ 生成发布文件
4. ✅ 提交到私有仓库并打标签
5. ✅ 提交到公开仓库并打标签
6. ✅ 推送到两个远程仓库

### 在 GitHub 上创建 Release

脚本完成后，访问：
https://github.com/ihugang/ClawWindowsDeskMcp/releases/new

1. 选择标签：`v0.3.0`
2. 填写标题：`v0.3.0 - 功能描述`
3. 上传可执行文件：
   - `../ClawDeskMCP-Release/bin/ClawDeskMCP-x64.exe`
   - `../ClawDeskMCP-Release/bin/ClawDeskMCP-x86.exe`
   - `../ClawDeskMCP-Release/bin/ClawDeskMCP-arm64.exe`（如果有）
4. 点击 "Publish release"

## 常用命令

### 查看当前版本

```bash
git describe --tags
```

### 查看仓库状态

```bash
# 私有仓库
git status

# 公开仓库
cd ../ClawDeskMCP-Release && git status && cd -
```

### 查看远程仓库

```bash
# 私有仓库
git remote -v

# 公开仓库
cd ../ClawDeskMCP-Release && git remote -v && cd -
```

### 只构建不发布

```bash
../scripts/build.sh
```

### 只生成发布文件

```bash
../scripts/release.sh
```

## 文件说明

- **`release.config`** - 仓库配置文件（包含仓库地址）
- **`../scripts/init-repos.sh`** - 初始化脚本（第一次使用）
- **`../scripts/build.sh`** - 构建脚本
- **`../scripts/release.sh`** - 发布脚本（生成发布文件）
- **`../scripts/auto-release.sh`** - 一键发布脚本（推荐使用）

## 目录结构

```
当前目录（私有仓库）
├── src/                    # 源代码
├── include/                # 头文件
├── ../scripts/build.sh                # 构建脚本
├── ../scripts/release.sh              # 发布脚本
├── ../scripts/auto-release.sh         # 一键发布脚本
├── ../scripts/init-repos.sh           # 初始化脚本
├── release.config          # 配置文件
└── ...

../ClawDeskMCP-Release（公开仓库）
├── bin/                    # 可执行文件
│   ├── ClawDeskMCP-x64.exe
│   ├── ClawDeskMCP-x86.exe
│   └── ClawDeskMCP-arm64.exe
├── docs/                   # 文档
├── README.md               # 用户指南
└── ...
```

## 故障排除

### 问题：git push 被拒绝

**原因**：可能是远程仓库已有内容，或者分支名称不匹配

**解决**：
```bash
# 如果远程仓库是空的，使用强制推送
git push -f origin main

# 如果远程仓库有内容，先拉取再推送
git pull origin main --rebase
git push origin main
```

### 问题：构建失败

**原因**：可能是 MinGW 工具链未安装

**解决**：
```bash
# 在 macOS 上安装 MinGW
brew install mingw-w64

# 验证安装
x86_64-w64-mingw32-g++ --version
```

### 问题：发布目录不存在

**原因**：还没有运行过 `../scripts/release.sh`

**解决**：
```bash
../scripts/release.sh
```

### 问题：无法推送到 GitHub

**原因**：可能是 SSH 密钥未配置，或使用了 HTTPS 地址但没有配置凭据

**解决**：

如果使用 HTTPS（当前配置）：
```bash
# 配置 GitHub 凭据
git config --global credential.helper osxkeychain

# 或者改用 SSH
git remote set-url origin git@github.com:ihugang/ClawWindowsDeskMcp_SourceCode.git
```

如果使用 SSH：
```bash
# 生成 SSH 密钥（如果还没有）
ssh-keygen -t ed25519 -C "your_email@example.com"

# 添加到 ssh-agent
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_ed25519

# 复制公钥并添加到 GitHub
cat ~/.ssh/id_ed25519.pub
# 访问 https://github.com/settings/keys 添加
```

## 检查清单

### 第一次设置
- [ ] 运行 `../scripts/init-repos.sh` 或手动初始化
- [ ] 确认私有仓库已推送
- [ ] 确认公开仓库已推送
- [ ] 在 GitHub 上查看两个仓库

### 每次发布
- [ ] 运行 `../scripts/auto-release.sh vX.X.X`
- [ ] 检查两个仓库都已推送
- [ ] 在 GitHub 上创建 Release
- [ ] 上传可执行文件
- [ ] 测试下载链接

## 需要帮助？

- 查看详细文档：`RELEASE_GUIDE.md`
- 查看快速参考：`RELEASE_QUICKREF.md`
- 查看方案对比：`RELEASE_OPTIONS.md`

## 仓库链接

- **私有仓库**：https://github.com/ihugang/ClawWindowsDeskMcp_SourceCode
- **公开仓库**：https://github.com/ihugang/ClawWindowsDeskMcp
