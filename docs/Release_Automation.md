# GitHub Release 自动化发布指南

## 概述

本项目使用 GitHub Actions 自动化构建和发布流程。当你推送一个版本标签（如 `v0.7.0`）时，GitHub Actions 会自动：

1. ✅ 交叉编译 Windows x64 和 x86 版本
2. ✅ 生成 SHA256 校验文件
3. ✅ 创建 GitHub Release
4. ✅ 上传所有构建产物
5. ✅ 从 CHANGELOG.md 提取 Release Notes

## 工作流配置

**文件位置**: `.github/workflows/release.yml`

### 触发条件

推送以 `v` 开头的标签：
```bash
git tag v0.7.0
git push origin v0.7.0
```

### 构建矩阵

- **平台**: Windows
- **架构**: x64, x86
- **编译器**: MinGW-w64
- **构建类型**: Release

### 构建产物

每个架构生成 4 个文件：

```
ClawDeskMCP-{version}-win-{arch}.exe
ClawDeskMCP-{version}-win-{arch}.exe.sha256
Updater-{version}-win-{arch}.exe
Updater-{version}-win-{arch}.exe.sha256
```

**示例**（v0.7.0）：
```
ClawDeskMCP-0.7.0-win-x64.exe
ClawDeskMCP-0.7.0-win-x64.exe.sha256
Updater-0.7.0-win-x64.exe
Updater-0.7.0-win-x64.exe.sha256
ClawDeskMCP-0.7.0-win-x86.exe
ClawDeskMCP-0.7.0-win-x86.exe.sha256
Updater-0.7.0-win-x86.exe
Updater-0.7.0-win-x86.exe.sha256
```

## 发布流程

### 1. 准备发布

#### 1.1 更新版本号

修改 `CMakeLists.txt`：
```cmake
project(ClawDeskMCP VERSION 0.7.0 LANGUAGES CXX)
```

#### 1.2 更新 CHANGELOG.md

添加新版本的更新日志：
```markdown
## [0.7.0] - 2026-02-05

### 新增

- ✅ 自动更新功能 - Phase 3：自动升级和重启
- ✅ Updater.exe 外置升级器
- ✅ 一键下载并安装更新

### 改进

- ✅ 完整的自动升级流程
- ✅ 备份和回滚机制

### 修复

- ✅ 修复资产命名过于严格的问题
- ✅ 修复线程安全问题
```

#### 1.3 提交更改

```bash
git add CMakeLists.txt CHANGELOG.md
git commit -m "Release v0.7.0"
git push origin main
```

### 2. 创建标签

```bash
# 创建标签
git tag -a v0.7.0 -m "Release version 0.7.0"

# 推送标签（触发 GitHub Actions）
git push origin v0.7.0
```

### 3. 等待构建

1. **访问 GitHub Actions**
   - 打开仓库页面
   - 点击 "Actions" 标签
   - 查看 "Build and Release" 工作流

2. **监控构建进度**
   - 构建 x64 版本（约 5-10 分钟）
   - 构建 x86 版本（约 5-10 分钟）
   - 创建 Release（约 1 分钟）

3. **构建完成**
   - ✅ 所有步骤显示绿色勾
   - ✅ Release 自动创建

### 4. 验证 Release

1. **访问 Releases 页面**
   ```
   https://github.com/ihugang/ClawDeskMCP/releases
   ```

2. **检查 Release 内容**
   - ✅ 标题：Version 0.7.0
   - ✅ Release Notes（从 CHANGELOG 提取）
   - ✅ 8 个附件文件（4 个 exe + 4 个 sha256）

3. **下载并测试**
   - 下载 `ClawDeskMCP-0.7.0-win-x64.exe`
   - 验证 SHA256
   - 在 Windows 上测试运行

## 手动发布（备选方案）

如果 GitHub Actions 不可用，可以手动发布：

### 1. 本地编译

```bash
# 编译 x64
./scripts/build.sh

# 或手动编译
mkdir -p build/x64
cd build/x64
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x64.cmake \
            -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

### 2. 重命名文件

```bash
cd build/x64
mv ClawDeskMCP.exe ClawDeskMCP-0.7.0-win-x64.exe
mv Updater.exe Updater-0.7.0-win-x64.exe
```

### 3. 生成 SHA256

```bash
sha256sum ClawDeskMCP-0.7.0-win-x64.exe > ClawDeskMCP-0.7.0-win-x64.exe.sha256
sha256sum Updater-0.7.0-win-x64.exe > Updater-0.7.0-win-x64.exe.sha256
```

### 4. 创建 GitHub Release

1. **访问 Releases 页面**
   ```
   https://github.com/ihugang/ClawDeskMCP/releases/new
   ```

2. **填写信息**
   - Tag: `v0.7.0`
   - Title: `Version 0.7.0`
   - Description: 从 CHANGELOG.md 复制内容

3. **上传文件**
   - 拖拽所有 `.exe` 和 `.sha256` 文件

4. **发布**
   - 点击 "Publish release"

## 高级配置

### 自动化测试

在发布前添加测试步骤（可选）：

```yaml
- name: Run tests
  run: |
    cd build/${{ matrix.arch }}
    # 运行测试（如果有）
    # ./run_tests.sh
```

### 代码签名

添加代码签名步骤（需要证书）：

```yaml
- name: Sign executables
  run: |
    # 使用 osslsigncode 或其他工具签名
    # osslsigncode sign -certs cert.pem -key key.pem \
    #   -n "ClawDesk MCP" -i https://github.com/ihugang/ClawDeskMCP \
    #   -in ClawDeskMCP.exe -out ClawDeskMCP-signed.exe
```

### 发布到多个平台

添加其他平台的构建（未来）：

```yaml
strategy:
  matrix:
    os: [ubuntu-latest, macos-latest, windows-latest]
    arch: [x64, arm64]
```

### Draft Release

如果想先创建草稿，手动审核后再发布：

```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    draft: true  # 改为 true
    prerelease: false
```

### Pre-release（Beta 版本）

发布 Beta 版本：

```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    draft: false
    prerelease: true  # 标记为 Pre-release
```

触发方式：
```bash
git tag v0.8.0-beta.1
git push origin v0.8.0-beta.1
```

## 版本号规范

遵循语义化版本（Semantic Versioning）：

```
v{MAJOR}.{MINOR}.{PATCH}[-{PRERELEASE}]
```

**示例**：
- `v0.7.0` - 正式版本
- `v0.8.0-beta.1` - Beta 版本
- `v0.8.0-rc.1` - Release Candidate
- `v1.0.0` - 主版本

**版本号含义**：
- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能新增
- **PATCH**: 向后兼容的问题修复

## 故障排查

### 问题 1：构建失败

**症状**：GitHub Actions 显示红色 ❌

**排查**：
1. 点击失败的步骤查看日志
2. 检查编译错误
3. 本地测试编译：`./scripts/build.sh`

**常见原因**：
- 代码编译错误
- 依赖缺失
- CMake 配置错误

### 问题 2：Release 未创建

**症状**：构建成功但没有 Release

**排查**：
1. 检查 GitHub Token 权限
2. 确认 `permissions: contents: write`
3. 查看 "Create Release" 步骤日志

**解决方法**：
- 确保仓库设置允许 Actions 创建 Release
- 检查 Token 权限

### 问题 3：文件上传失败

**症状**：Release 创建但文件缺失

**排查**：
1. 检查文件路径是否正确
2. 确认 artifacts 下载成功
3. 查看 "Upload artifacts" 步骤

**解决方法**：
- 验证文件路径匹配
- 检查 glob 模式

### 问题 4：SHA256 格式错误

**症状**：自动更新无法验证 SHA256

**排查**：
1. 下载 `.sha256` 文件查看格式
2. 确认格式为：`<hash> *<filename>`

**正确格式**：
```
a1b2c3d4e5f6... *ClawDeskMCP-0.7.0-win-x64.exe
```

## 最佳实践

### 1. 发布前检查清单

- [ ] 更新版本号（CMakeLists.txt）
- [ ] 更新 CHANGELOG.md
- [ ] 本地编译测试
- [ ] 运行所有测试
- [ ] 提交所有更改
- [ ] 创建并推送标签

### 2. Release Notes 规范

**好的 Release Notes**：
```markdown
## 新增功能
- 自动更新功能
- 外置升级器

## 改进
- 优化下载速度
- 改进用户界面

## 修复
- 修复线程安全问题
- 修复资产命名问题

## 已知问题
- 暂无

## 升级说明
1. 下载新版本
2. 替换旧版本
3. 重启程序
```

### 3. 标签管理

**删除错误的标签**：
```bash
# 删除本地标签
git tag -d v0.7.0

# 删除远程标签
git push origin :refs/tags/v0.7.0
```

**重新创建标签**：
```bash
git tag -a v0.7.0 -m "Release version 0.7.0"
git push origin v0.7.0
```

### 4. 版本发布节奏

**建议**：
- **Patch 版本**：每 1-2 周（修复 bug）
- **Minor 版本**：每 1-2 月（新功能）
- **Major 版本**：每 6-12 月（重大变更）

### 5. Beta 测试流程

1. **创建 Beta 版本**
   ```bash
   git tag v0.8.0-beta.1
   git push origin v0.8.0-beta.1
   ```

2. **收集反馈**（1-2 周）

3. **修复问题并发布 Beta 2**
   ```bash
   git tag v0.8.0-beta.2
   git push origin v0.8.0-beta.2
   ```

4. **发布正式版本**
   ```bash
   git tag v0.8.0
   git push origin v0.8.0
   ```

## 安全注意事项

### 1. GitHub Token

- ✅ 使用内置 `GITHUB_TOKEN`（自动提供）
- ❌ 不要在代码中硬编码 Token
- ❌ 不要提交 Token 到仓库

### 2. 代码签名

**推荐**：
- 使用 EV 代码签名证书
- 证书存储在 GitHub Secrets
- 不要提交证书到仓库

### 3. 构建环境

- ✅ 使用官方 GitHub Actions runners
- ✅ 固定依赖版本
- ✅ 验证所有下载的依赖

## 监控和通知

### 1. 邮件通知

GitHub 会自动发送构建状态邮件。

### 2. Slack/Discord 通知（可选）

添加通知步骤：

```yaml
- name: Notify Slack
  if: success()
  uses: 8398a7/action-slack@v3
  with:
    status: ${{ job.status }}
    text: 'Release ${{ steps.get_info.outputs.VERSION }} published!'
  env:
    SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK }}
```

### 3. 发布统计

在 GitHub Insights 中查看：
- 下载次数
- 流量统计
- 用户分布

## 总结

✅ **自动化发布的优势**：
- 节省时间（无需手动编译和上传）
- 减少错误（自动化流程）
- 一致性（每次构建相同）
- 可追溯（完整的构建日志）

✅ **发布流程**：
```
更新代码 → 提交 → 创建标签 → 推送标签 → 自动构建 → 自动发布
```

✅ **下次发布只需**：
```bash
# 1. 更新版本号和 CHANGELOG
# 2. 提交更改
git add .
git commit -m "Release v0.8.0"
git push

# 3. 创建并推送标签
git tag v0.8.0
git push origin v0.8.0

# 4. 等待自动构建完成（10-15 分钟）
# 5. 完成！
```

---

**相关文档**：
- `docs/AutoUpdate_Complete.md` - 自动更新功能文档
- `docs/AutoUpdate_Testing_Guide.md` - 测试指南
- `.github/workflows/release.yml` - 工作流配置
