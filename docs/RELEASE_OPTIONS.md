# 发布方案选择指南

## 方案对比表

| 特性 | 双仓库方案 | 单仓库+外部托管 | 单仓库公开 |
|------|-----------|----------------|-----------|
| 源代码保护 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ❌ |
| 维护复杂度 | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 用户体验 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| 团队协作 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ❌ |
| 成本 | 免费 | 需要托管费用 | 免费 |

## 推荐方案：双仓库 + 自动化

### 仓库设置

**私有仓库**：`ClawDeskMCP`（或 `ClawDeskMCP-Private`）
- 包含：所有源代码、构建脚本、开发文档
- 访问：仅团队成员
- 地址示例：`github.com/YourOrg/ClawDeskMCP-Private`

**公开仓库**：`ClawDeskMCP-Release`
- 包含：编译好的程序、用户文档、MCP 协议说明
- 访问：所有人
- 地址示例：`github.com/YourOrg/ClawDeskMCP-Release`

### 自动化发布流程

使用我提供的 `../scripts/release.sh` 脚本，只需要 3 个命令：

```bash
# 1. 构建
../scripts/build.sh

# 2. 发布
../scripts/release.sh

# 3. 推送到公开仓库
cd ../ClawDeskMCP-Release && git add . && git commit -m "Release v0.2.0" && git push
```

### 进一步自动化（可选）

如果你想更自动化，可以创建一个一键发布脚本：

```bash
#!/bin/bash
# ../scripts/auto-release.sh - 一键发布脚本

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "用法: ../scripts/auto-release.sh v0.2.0"
    exit 1
fi

echo "开始发布 ${VERSION}..."

# 1. 更新版本号
sed -i '' "s/VERSION=\".*\"/VERSION=\"${VERSION}\"/" ../scripts/release.sh
sed -i '' "s/VERSION [0-9.]*/VERSION ${VERSION#v}/" CMakeLists.txt

# 2. 构建
../scripts/build.sh || exit 1

# 3. 发布
../scripts/release.sh || exit 1

# 4. 提交私有仓库
git add .
git commit -m "Release ${VERSION}"
git tag ${VERSION}
git push origin main --tags

# 5. 提交公开仓库
cd ../ClawDeskMCP-Release
git add .
git commit -m "Release ${VERSION}"
git tag ${VERSION}
git push origin main --tags

echo "✓ 发布完成！"
echo "请访问 GitHub 创建 Release 并上传可执行文件"
```

使用方法：

```bash
../scripts/auto-release.sh v0.2.0
```

## 其他方案

### 方案 A：单仓库 + 云存储

如果你不想维护两个 GitHub 仓库，可以：

1. **私有 GitHub 仓库**：存放源代码
2. **云存储**：存放编译好的程序
   - 阿里云 OSS
   - 腾讯云 COS
   - AWS S3
   - 七牛云

3. **官网/文档站**：提供下载链接和文档
   - GitHub Pages（可以单独建一个公开仓库）
   - Vercel/Netlify（免费托管）

**优点**：
- 只需要维护一个 GitHub 仓库
- 下载速度可能更快（CDN 加速）
- 可以统计下载量

**缺点**：
- 需要额外的托管费用（但通常很便宜）
- 用户无法通过 GitHub Issues 反馈问题

### 方案 B：单仓库 + GitHub Pages

1. **私有 GitHub 仓库**：存放源代码
2. **GitHub Pages**（公开）：托管文档和下载链接
   - 可以在同一个仓库的 `gh-pages` 分支
   - 或者单独建一个公开仓库

**优点**：
- 免费
- 文档可以公开访问
- 可以使用自定义域名

**缺点**：
- GitHub Pages 有大小限制（1GB）
- 大文件下载速度可能较慢

## 我的最终推荐

对于你的项目，我强烈推荐：

### 🎯 双仓库方案 + 自动化脚本

**理由**：
1. ✅ 源代码完全保护
2. ✅ 用户体验最好（直接在 GitHub 下载）
3. ✅ 可以接收用户 Issues 和反馈
4. ✅ 完全免费
5. ✅ 通过自动化脚本，维护成本很低

**实施步骤**：
1. 创建两个 GitHub 仓库
2. 使用我提供的 `../scripts/release.sh` 脚本
3. 可选：创建 `../scripts/auto-release.sh` 进一步自动化

**时间成本**：
- 初次设置：30 分钟
- 每次发布：5 分钟（运行 3 个命令）

## 决策建议

选择方案时考虑：

1. **如果你重视用户体验和社区反馈** → 双仓库方案
2. **如果你已有官网和下载站** → 单仓库 + 云存储
3. **如果你想最简单** → 单仓库 + GitHub Pages

对于商业软件，我建议选择**双仓库方案**，这是业界标准做法。
