# 发布流程快速参考

## 一次性设置

### 1. 初始化私有仓库（源代码）

```bash
cd /Volumes/SN770/Downloads/Dev/2026/Products/WinAgent

git init
git add .
git commit -m "Initial commit: ClawDesk MCP Server"
git remote add origin git@github.com:YourUsername/ClawDeskMCP-Private.git
git branch -M main
git push -u origin main
```

### 2. 创建公开仓库

在 GitHub 上创建新仓库：
- 仓库名：`ClawDeskMCP-Release`
- 可见性：**Public**
- 不要初始化 README、.gitignore 或 LICENSE（我们会自己创建）

## 每次发布流程

### 步骤 1: 更新版本号

编辑 `../scripts/release.sh`，修改版本号：

```bash
VERSION="0.3.0"  # 改为新版本号
```

编辑 `CMakeLists.txt`，修改版本号：

```cmake
project(ClawDeskMCP VERSION 0.3.0 LANGUAGES CXX)
```

### 步骤 2: 构建程序

```bash
../scripts/build.sh
```

### 步骤 3: 运行发布脚本

```bash
../scripts/release.sh
```

### 步骤 4: 提交到公开仓库

```bash
cd ../ClawDeskMCP-Release

# 如果是第一次
git init
git add .
git commit -m "Release v0.3.0"
git tag v0.3.0
git remote add origin git@github.com:YourUsername/ClawDeskMCP-Release.git
git branch -M main
git push -u origin main --tags

# 如果已经初始化过
git add .
git commit -m "Release v0.3.0"
git tag v0.3.0
git push origin main --tags
```

### 步骤 5: 在 GitHub 上创建 Release

1. 访问 https://github.com/YourUsername/ClawDeskMCP-Release/releases
2. 点击 "Create a new release"
3. 选择标签 `v0.3.0`
4. 标题：`v0.3.0 - 功能描述`
5. 上传文件：
   - `bin/ClawDeskMCP-x64.exe`
   - `bin/ClawDeskMCP-x86.exe`
   - `bin/ClawDeskMCP-arm64.exe`（如果有）
6. 点击 "Publish release"

### 步骤 6: 提交私有仓库

```bash
cd /Volumes/SN770/Downloads/Dev/2026/Products/WinAgent

git add .
git commit -m "Release v0.3.0"
git tag v0.3.0
git push origin main --tags
```

## 常用命令

### 查看当前版本

```bash
git describe --tags
```

### 查看所有标签

```bash
git tag -l
```

### 删除错误的标签

```bash
# 删除本地标签
git tag -d v0.3.0

# 删除远程标签
git push origin :refs/tags/v0.3.0
```

### 查看发布目录大小

```bash
du -sh ../ClawDeskMCP-Release
```

## 检查清单

发布前检查：

- [ ] 更新了 `../scripts/release.sh` 中的版本号
- [ ] 更新了 `CMakeLists.txt` 中的版本号
- [ ] 运行了 `../scripts/build.sh` 并成功构建
- [ ] 运行了 `../scripts/release.sh` 并检查输出
- [ ] 检查了发布目录中的文件是否完整
- [ ] 确认没有包含源代码
- [ ] 确认没有包含敏感信息

发布后检查：

- [ ] 公开仓库已推送成功
- [ ] GitHub Release 已创建
- [ ] 可执行文件已上传
- [ ] 文档链接正常
- [ ] 私有仓库已打标签

## 故障排除

### 问题：../scripts/release.sh 提示构建产物不存在

**解决**：先运行 `../scripts/build.sh` 构建程序

### 问题：git push 被拒绝

**解决**：检查远程仓库地址是否正确，或使用 `git push -f`（谨慎使用）

### 问题：GitHub Release 上传失败

**解决**：手动上传文件，或检查文件大小是否超过限制（2GB）

### 问题：发布目录包含了源代码

**解决**：检查 `../scripts/release.sh` 脚本，确保只复制了必要的文件

## 联系方式

如有问题，请联系开发团队。
