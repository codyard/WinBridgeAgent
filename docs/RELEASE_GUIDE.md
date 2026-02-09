# 发布指南：公开仓库 vs 私有仓库

本文档说明如何将 ClawDesk MCP Server 的发布版本和文档发布到公开 Git 仓库，同时保持源代码在私有仓库中。

## 仓库结构

### 私有仓库（源代码）

**仓库名称建议**: `ClawDeskMCP` 或 `ClawDeskMCP-Private`

**包含内容**:
- 所有源代码（`src/`, `include/`）
- 构建脚本（`../scripts/build.sh`, `CMakeLists.txt`）
- 工具链配置（`toolchain-*.cmake`）
- 第三方库（`third_party/`）
- 测试代码（`tests/`）
- 开发文档（`SETUP.md`, `AGENTS.md`, `CLAUDE.md`）
- 规范文档（`.kiro/specs/`）
- 发布脚本（`../scripts/release.sh`）

**访问权限**: 仅开发团队

### 公开仓库（发布版本）

**仓库名称建议**: `ClawDeskMCP-Release` 或 `ClawDeskMCP-Releases`

**包含内容**:
- 编译好的可执行文件（`bin/ClawDeskMCP-*.exe`）
- 用户文档（`README.md`, `docs/`）
- MCP 协议说明（`docs/MCP-Requirements.md`）
- 配置模板（`config.template.json`）
- 许可证文件（`LICENSE`）
- 版本信息（`VERSION.txt`）

**不包含**:
- 源代码
- 构建脚本
- 开发文档
- 第三方库源码

**访问权限**: 公开

## 操作步骤

### 第一步：初始化私有仓库

在当前项目目录（源代码目录）：

```bash
# 1. 初始化 git 仓库
git init

# 2. 创建 .gitignore
cat > .gitignore << 'EOF'
# 构建产物
build-*/
*.exe
*.o
*.obj

# 运行时文件
config.json
runtime.json
usage.json
logs/
screenshots/

# IDE
.vscode/
.idea/
*.swp
*.swo

# macOS
.DS_Store

# Windows
Thumbs.db
desktop.ini

# 测试客户端
test-client/node_modules/
test-client/dist/
EOF

# 3. 添加所有文件
git add .

# 4. 创建初始提交
git commit -m "Initial commit: ClawDesk MCP Server source code"

# 5. 添加远程仓库（替换为你的私有仓库地址）
git remote add origin git@github.com:YourUsername/ClawDeskMCP-Private.git

# 6. 推送到远程
git branch -M main
git push -u origin main
```

### 第二步：构建发布版本

```bash
# 1. 构建所有架构
../scripts/build.sh

# 2. 验证构建产物
ls -lh build/x64/ClawDeskMCP.exe
ls -lh build/x86/ClawDeskMCP.exe
```

### 第三步：准备发布目录

```bash
# 运行发布脚本
../scripts/release.sh
```

这个脚本会：
1. 检查构建产物是否存在
2. 创建 `../ClawDeskMCP-Release` 目录
3. 复制可执行文件到 `bin/` 目录
4. 复制用户文档到 `docs/` 目录
5. 复制 MCP 协议说明
6. 创建版本信息文件
7. 创建公开仓库专用的 README 和 .gitignore

### 第四步：初始化公开仓库

```bash
# 1. 进入发布目录
cd ../ClawDeskMCP-Release

# 2. 初始化 git 仓库
git init

# 3. 添加所有文件
git add .

# 4. 创建初始提交
git commit -m "Release v0.2.0"

# 5. 创建版本标签
git tag v0.2.0

# 6. 添加远程仓库（替换为你的公开仓库地址）
git remote add origin git@github.com:YourUsername/ClawDeskMCP-Release.git

# 7. 推送到远程
git branch -M main
git push -u origin main --tags
```

### 第五步：在 GitHub 上创建 Release

1. 访问公开仓库的 GitHub 页面
2. 点击 "Releases" → "Create a new release"
3. 选择标签 `v0.2.0`
4. 填写 Release 标题：`v0.2.0 - Initial Release`
5. 填写 Release 说明（参考下面的模板）
6. 上传可执行文件作为附件：
   - `ClawDeskMCP-x64.exe`
   - `ClawDeskMCP-x86.exe`
   - `ClawDeskMCP-arm64.exe`（如果有）
7. 点击 "Publish release"

#### Release 说明模板

```markdown
## ClawDesk MCP Server v0.2.0

### 功能特性

- ✅ MCP 协议支持
- ✅ HTTP API 接口
- ✅ 实时监控 Dashboard
- ✅ 系统托盘界面
- ✅ 文件操作（磁盘枚举、目录列表、文件读取、内容搜索）
- ✅ 剪贴板操作
- ✅ 安全白名单机制
- ✅ 审计日志记录

### 系统要求

- Windows 10/11 (x64, x86, ARM64)
- 无需额外运行时依赖

### 下载

选择适合你系统的版本：

- **ClawDeskMCP-x64.exe**: 64 位 Windows 系统（推荐）
- **ClawDeskMCP-x86.exe**: 32 位 Windows 系统
- **ClawDeskMCP-arm64.exe**: ARM64 Windows 系统

### 快速开始

1. 下载对应版本的可执行文件
2. 双击运行，程序会在系统托盘显示图标
3. 首次运行会自动生成 `config.json` 配置文件
4. 右键点击托盘图标可以查看状态和配置

### 文档

- [完整使用指南](README.md)
- [HTTP API 文档](docs/API.md)
- [Dashboard 使用指南](docs/Dashboard.md)
- [MCP 协议说明](docs/MCP-Requirements.md)
- [防火墙配置指南](FIREWALL.md)

### 已知问题

无

### 更新日志

- 初始发布版本
```

## 后续发布流程

当你需要发布新版本时：

### 在私有仓库中

```bash
# 1. 更新版本号（在 CMakeLists.txt 中）
# 2. 提交代码更改
git add .
git commit -m "Bump version to 0.3.0"
git tag v0.3.0
git push origin main --tags

# 3. 构建新版本
../scripts/build.sh

# 4. 运行发布脚本（需要先更新 ../scripts/release.sh 中的 VERSION 变量）
../scripts/release.sh
```

### 在公开仓库中

```bash
# 1. 进入发布目录
cd ../ClawDeskMCP-Release

# 2. 添加新文件
git add .
git commit -m "Release v0.3.0"
git tag v0.3.0

# 3. 推送到远程
git push origin main --tags

# 4. 在 GitHub 上创建新的 Release
```

## 自动化发布（可选）

你可以使用 GitHub Actions 自动化发布流程。在私有仓库中创建 `.github/workflows/release.yml`：

```yaml
name: Build and Release

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install MinGW
        run: |
          sudo apt-get update
          sudo apt-get install -y mingw-w64

      - name: Build
        run: ../scripts/build.sh

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            build/x64/ClawDeskMCP.exe
            build/x86/ClawDeskMCP.exe
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

## 安全注意事项

### 私有仓库

1. **不要提交敏感信息**:
   - API 密钥
   - 许可证生成密钥
   - 测试用的 Token

2. **使用 .gitignore**:
   - 确保 `config.json` 不被提交
   - 确保构建产物不被提交

3. **访问控制**:
   - 只授权开发团队成员访问
   - 使用 SSH 密钥而非密码

### 公开仓库

1. **不要包含源代码**:
   - 仅发布编译后的二进制文件
   - 不包含构建脚本和工具链配置

2. **文档审查**:
   - 确保文档中没有内部信息
   - 不暴露内部架构细节

3. **许可证**:
   - 明确许可证条款
   - 保护商业权益

## 目录结构对比

### 私有仓库

```
ClawDeskMCP/
├── .git/
├── .gitignore
├── CMakeLists.txt
├── ../scripts/build.sh
├── ../scripts/release.sh                    # 发布脚本
├── toolchain-*.cmake
├── src/                          # 源代码
├── include/                      # 头文件
├── tests/                        # 测试
├── third_party/                  # 第三方库
├── resources/
├── .kiro/specs/                  # 规范文档
├── SETUP.md                      # 开发文档
├── AGENTS.md                     # 开发文档
├── CLAUDE.md                     # 开发文档
├── README.md                     # 用户文档
├── LICENSE
└── docs/
```

### 公开仓库

```
ClawDeskMCP-Release/
├── .git/
├── .gitignore
├── README-PUBLIC.md              # 公开仓库首页
├── README.md                     # 用户使用指南
├── LICENSE
├── VERSION.txt                   # 版本信息
├── config.template.json          # 配置模板
├── bin/
│   ├── ClawDeskMCP-x64.exe      # 可执行文件
│   ├── ClawDeskMCP-x86.exe
│   └── ClawDeskMCP-arm64.exe
└── docs/
    ├── API.md                    # API 文档
    ├── Dashboard.md              # Dashboard 文档
    ├── MCP-Requirements.md       # MCP 协议说明
    └── FIREWALL.md               # 防火墙指南
```

## 常见问题

### Q: 如何更新公开仓库中的文档？

A: 在私有仓库中修改文档后，重新运行 `../scripts/release.sh`，然后在公开仓库中提交更改。

### Q: 如何处理 Bug 修复？

A:
1. 在私有仓库中修复 Bug
2. 提交代码并打上新的版本标签
3. 重新构建并运行 `../scripts/release.sh`
4. 在公开仓库中发布新版本

### Q: 用户如何报告问题？

A: 在公开仓库中启用 Issues 功能，用户可以在那里报告问题。开发团队在私有仓库中修复后发布新版本。

### Q: 如何保护二进制文件不被逆向？

A:
1. 使用 Release 模式编译（已启用 `-O3 -s`）
2. 考虑使用 VMProtect 或 Themida 加壳
3. 关键算法使用代码混淆

## 总结

通过这种双仓库策略，你可以：

✅ 保护源代码知识产权
✅ 公开发布程序和文档
✅ 方便用户下载和使用
✅ 接收用户反馈和 Bug 报告
✅ 保持开发流程的灵活性

记住：**永远不要在公开仓库中提交源代码！**
