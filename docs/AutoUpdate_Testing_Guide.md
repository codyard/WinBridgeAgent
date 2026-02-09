# 自动升级功能测试指南

## 测试环境准备

### 1. 编译程序

在 macOS 上交叉编译 Windows 版本：

```bash
# 完整构建（包括主程序和 Updater）
./scripts/build.sh

# 或手动编译
mkdir -p build/x64
cd build/x64
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x64.cmake \
            -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

**构建产物**：
```
build/x64/
├── ClawDeskMCP.exe      # 主程序（当前版本 v0.7.0）
└── Updater.exe          # 升级器
```

### 2. 准备测试环境

#### Windows 测试机器
- Windows 10/11 x64 或 ARM64
- 网络连接（用于下载更新）
- 管理员权限（可选，用于某些目录）

#### 测试目录结构
```
C:\TestApp\
├── ClawDeskMCP.exe      # 主程序
├── Updater.exe          # 升级器
├── config.json          # 配置文件
└── logs\                # 日志目录
    ├── audit.log
    └── updater.log      # 升级日志
```

### 3. 配置文件设置

编辑 `config.json`：

```json
{
  "auto_update": {
    "enabled": true,
    "check_on_startup": true,
    "update_channel": "stable",
    "github_repo": "ihugang/ClawDeskMCP",
    "last_check": "",
    "update_check_interval_hours": 24,
    "skipped_version": ""
  }
}
```

**配置说明**：
- `enabled`: 设为 `true` 启用自动更新
- `check_on_startup`: 设为 `true` 启动时自动检查
- `update_channel`: `"stable"` 或 `"beta"`

---

## 测试场景

### 场景 1：手动检查更新（基础功能）

**目的**：测试手动检查更新功能

**步骤**：

1. **启动程序**
   ```
   双击 ClawDeskMCP.exe
   ```

2. **打开托盘菜单**
   - 右键点击系统托盘图标
   - 找到"检查更新"菜单项

3. **点击"检查更新"**
   - 观察是否显示"正在检查更新..."
   - 等待检查完成

**预期结果**：

- ✅ 如果有新版本：显示更新对话框
  ```
  A new version is available!
  
  Current: 0.7.0
  Latest: 0.8.0
  
  Release Name
  
  Do you want to download and install the update now?
  [Yes] [No] [Cancel]
  ```

- ✅ 如果已是最新：显示提示
  ```
  You are running the latest version: 0.7.0
  ```

- ✅ 如果网络错误：显示错误信息
  ```
  Failed to check for updates: Network error
  ```

---

### 场景 2：下载更新文件

**目的**：测试文件下载功能

**前提**：场景 1 发现有新版本

**步骤**：

1. **点击"Yes"下载**
   - 观察下载进度窗口是否出现

2. **观察下载进度**
   - 进度条是否更新
   - 下载速度是否显示（MB/s）
   - 文件大小是否正确

3. **等待下载完成**
   - 观察 SHA256 验证过程
   - 等待"下载完成"提示

**预期结果**：

- ✅ 下载进度窗口正常显示
- ✅ 进度条实时更新
- ✅ 下载速度显示正常
- ✅ SHA256 验证成功
- ✅ 显示"Download completed successfully!"

**测试下载到的文件**：
```
C:\Users\<用户名>\AppData\Local\Temp\ClawDeskMCP-0.8.0-win-x64.exe
```

---

### 场景 3：断点续传测试

**目的**：测试下载中断后的续传功能

**步骤**：

1. **开始下载**
   - 点击"Yes"开始下载

2. **中途取消**
   - 下载到 30-50% 时点击"Cancel"

3. **重新下载**
   - 再次点击"检查更新"
   - 点击"Yes"重新下载

**预期结果**：

- ✅ 第二次下载从上次中断的位置继续
- ✅ 不重新下载已下载的部分
- ✅ 下载速度正常

**验证方法**：
- 观察下载进度是否从 0% 开始（应该从中断位置开始）
- 检查临时文件大小是否保留

---

### 场景 4：完整升级流程

**目的**：测试完整的自动升级和重启

**步骤**：

1. **下载完成后点击"Yes"安装**
   ```
   Download completed successfully!
   
   Install now and restart the application?
   [Yes] [No]
   ```

2. **观察升级过程**
   - 主程序是否退出
   - Updater.exe 是否启动
   - 升级进度窗口是否显示

3. **等待升级完成**
   - 观察进度提示
   - 等待新版本自动启动

4. **验证升级结果**
   - 新版本是否正常启动
   - 版本号是否更新
   - 功能是否正常

**预期结果**：

- ✅ 主程序正常退出
- ✅ Updater.exe 启动并显示进度
- ✅ 备份文件生成（ClawDeskMCP.exe.backup）
- ✅ 新版本自动启动
- ✅ 版本号更新正确

**升级日志**：
```
C:\TestApp\logs\updater.log
```

查看日志内容：
```
[2026-02-05 12:00:00] === ClawDesk MCP Updater v1.0.0 ===
[2026-02-05 12:00:00] New file: C:\Users\...\Temp\ClawDeskMCP-0.8.0-win-x64.exe
[2026-02-05 12:00:00] Target file: C:\TestApp\ClawDeskMCP.exe
[2026-02-05 12:00:00] Process name: ClawDeskMCP.exe
[2026-02-05 12:00:01] Waiting for process to exit: ClawDeskMCP.exe
[2026-02-05 12:00:02] Process exited: ClawDeskMCP.exe
[2026-02-05 12:00:02] Backing up: C:\TestApp\ClawDeskMCP.exe -> C:\TestApp\ClawDeskMCP.exe.backup
[2026-02-05 12:00:02] Backup successful
[2026-02-05 12:00:02] Replacing: C:\TestApp\ClawDeskMCP.exe with new version
[2026-02-05 12:00:02] Replace successful
[2026-02-05 12:00:02] Starting process: C:\TestApp\ClawDeskMCP.exe
[2026-02-05 12:00:02] Process started successfully
[2026-02-05 12:00:02] Update completed successfully
[2026-02-05 12:00:02] Updater finished
```

---

### 场景 5：启动时自动检查

**目的**：测试启动时自动检查更新功能

**前提**：配置文件中 `check_on_startup = true`

**步骤**：

1. **启动程序**
   ```
   双击 ClawDeskMCP.exe
   ```

2. **等待 2-3 秒**
   - 观察是否自动检查更新

3. **如果有新版本**
   - 观察是否自动弹出更新对话框

**预期结果**：

- ✅ 程序启动后 2 秒自动检查更新
- ✅ 不阻塞程序启动
- ✅ 有新版本时自动提示

**日志验证**：
```
C:\TestApp\logs\audit.log
```

查找类似日志：
```
[auto_update] Checking for updates on startup...
```

---

### 场景 6：配置开关测试

**目的**：测试配置开关是否生效

#### 6.1 禁用自动更新

**步骤**：

1. **修改配置**
   ```json
   {
     "auto_update": {
       "enabled": false
     }
   }
   ```

2. **重启程序**

3. **点击"检查更新"**

**预期结果**：

- ✅ 显示提示：
  ```
  Auto-update is disabled in configuration.
  
  Please enable it in config.json to check for updates.
  ```

#### 6.2 禁用启动检查

**步骤**：

1. **修改配置**
   ```json
   {
     "auto_update": {
       "enabled": true,
       "check_on_startup": false
     }
   }
   ```

2. **重启程序**

3. **等待 5 秒**

**预期结果**：

- ✅ 启动时不自动检查更新
- ✅ 手动点击"检查更新"仍可用

---

### 场景 7：Beta 版本测试

**目的**：测试 Beta 通道切换

**步骤**：

1. **修改配置**
   ```json
   {
     "auto_update": {
       "update_channel": "beta"
     }
   }
   ```

2. **检查更新**
   - 点击"检查更新"

**预期结果**：

- ✅ 检查 Beta 版本（包括 Pre-release）
- ✅ 显示 Beta 版本信息

---

### 场景 8：升级失败回滚

**目的**：测试升级失败时的回滚机制

**模拟方法**：

#### 方法 1：文件占用

1. **下载新版本**
2. **在安装前打开 ClawDeskMCP.exe 文件**
   - 用记事本或其他程序打开
3. **点击"Yes"安装**

**预期结果**：

- ✅ 升级失败
- ✅ 自动回滚到旧版本
- ✅ 显示错误提示
- ✅ 程序仍可正常运行

#### 方法 2：删除 Updater.exe

1. **删除 Updater.exe**
2. **尝试安装更新**

**预期结果**：

- ✅ 显示错误：
  ```
  Failed to start updater. Please install manually.
  ```

---

### 场景 9：资产命名兼容性测试

**目的**：测试多种资产命名格式的兼容性

**测试资产命名**：

在 GitHub Release 中上传不同命名格式的文件：

1. **带版本号**：`ClawDeskMCP-0.8.0-win-x64.exe`
2. **无版本号**：`ClawDeskMCP-win-x64.exe`
3. **简化格式**：`ClawDeskMCP-x64.exe`

**步骤**：

1. 分别测试每种命名格式
2. 检查更新
3. 观察是否能正确识别

**预期结果**：

- ✅ 所有格式都能正确识别
- ✅ 优先匹配带版本号的格式
- ✅ SHA256 文件正确匹配

---

### 场景 10：并发检查测试

**目的**：测试快速连续点击的处理

**步骤**：

1. **快速连续点击"检查更新"**
   - 连续点击 5-10 次

**预期结果**：

- ✅ 第一次点击开始检查
- ✅ 后续点击显示：
  ```
  Already checking for updates...
  ```
- ✅ 不会启动多个检查任务
- ✅ 无数据竞争或崩溃

---

## 测试检查清单

### 基础功能

- [ ] 手动检查更新
- [ ] 发现新版本
- [ ] 已是最新版本
- [ ] 网络错误处理
- [ ] 下载进度显示
- [ ] SHA256 验证
- [ ] 完整升级流程
- [ ] 自动重启

### 高级功能

- [ ] 断点续传
- [ ] 取消下载
- [ ] 启动时检查
- [ ] 配置开关生效
- [ ] Beta 通道切换
- [ ] 升级失败回滚
- [ ] 资产命名兼容性

### 线程安全

- [ ] 快速连续点击
- [ ] 后台检查不阻塞 UI
- [ ] 无数据竞争
- [ ] 无内存泄漏

### 用户体验

- [ ] 进度显示清晰
- [ ] 错误提示友好
- [ ] 操作流程顺畅
- [ ] 日志记录完整

---

## 模拟测试环境

### 方法 1：使用本地 HTTP 服务器

如果没有真实的 GitHub Release，可以模拟：

1. **创建模拟 Release JSON**

```json
{
  "tag_name": "v0.8.0",
  "name": "Version 0.8.0",
  "prerelease": false,
  "assets": [
    {
      "name": "ClawDeskMCP-0.8.0-win-x64.exe",
      "browser_download_url": "http://localhost:8000/ClawDeskMCP-0.8.0-win-x64.exe"
    },
    {
      "name": "ClawDeskMCP-0.8.0-win-x64.exe.sha256",
      "browser_download_url": "http://localhost:8000/ClawDeskMCP-0.8.0-win-x64.exe.sha256"
    }
  ]
}
```

2. **启动本地服务器**

```bash
# Python 3
python3 -m http.server 8000
```

3. **修改代码指向本地服务器**（临时测试）

### 方法 2：创建测试 Release

在你的 GitHub 仓库中创建测试 Release：

1. **编译新版本**
   - 修改版本号为 0.8.0
   - 重新编译

2. **生成 SHA256**
   ```bash
   sha256sum ClawDeskMCP.exe > ClawDeskMCP-0.8.0-win-x64.exe.sha256
   ```

3. **创建 GitHub Release**
   - 标签：v0.8.0
   - 上传文件和 SHA256

4. **测试更新**

---

## 常见问题排查

### 问题 1：检查更新失败

**可能原因**：
- 网络连接问题
- GitHub API 限制
- 仓库名称错误

**排查方法**：
```bash
# 检查网络
ping api.github.com

# 检查 API 访问
curl https://api.github.com/repos/ihugang/ClawDeskMCP/releases/latest
```

### 问题 2：下载失败

**可能原因**：
- 磁盘空间不足
- 网络中断
- URL 无效

**排查方法**：
- 检查磁盘空间
- 查看日志文件
- 手动下载测试

### 问题 3：SHA256 验证失败

**可能原因**：
- 文件损坏
- SHA256 文件格式错误
- 下载不完整

**排查方法**：
```bash
# 手动验证 SHA256
sha256sum ClawDeskMCP-0.8.0-win-x64.exe
# 对比 .sha256 文件内容
```

### 问题 4：升级失败

**可能原因**：
- 文件被占用
- 权限不足
- Updater.exe 缺失

**排查方法**：
- 查看 updater.log
- 检查文件权限
- 确认 Updater.exe 存在

### 问题 5：新版本未启动

**可能原因**：
- 文件路径错误
- 新版本损坏
- 防病毒软件阻止

**排查方法**：
- 手动启动新版本
- 检查防病毒日志
- 查看 Windows 事件查看器

---

## 性能测试

### 1. 启动性能

**测试**：启动时检查更新是否影响启动速度

**方法**：
- 测量启动时间（从双击到托盘图标出现）
- 对比启用/禁用启动检查的差异

**预期**：
- 启动延迟 < 3 秒
- 不阻塞主程序启动

### 2. 内存占用

**测试**：检查更新和下载时的内存占用

**方法**：
- 使用任务管理器监控内存
- 记录峰值内存占用

**预期**：
- 检查更新：< 10MB 额外内存
- 下载文件：< 50MB 额外内存

### 3. CPU 占用

**测试**：后台检查和下载的 CPU 占用

**方法**：
- 使用任务管理器监控 CPU
- 记录平均 CPU 占用

**预期**：
- 检查更新：< 5% CPU
- 下载文件：< 10% CPU

---

## 测试报告模板

```markdown
# 自动升级功能测试报告

## 测试信息
- 测试日期：2026-02-05
- 测试人员：[姓名]
- 测试版本：v0.7.0
- 测试环境：Windows 11 x64

## 测试结果

### 场景 1：手动检查更新
- 状态：✅ 通过
- 备注：功能正常

### 场景 2：下载更新文件
- 状态：✅ 通过
- 备注：下载速度正常，进度显示准确

### 场景 3：断点续传
- 状态：✅ 通过
- 备注：续传功能正常

### 场景 4：完整升级流程
- 状态：✅ 通过
- 备注：升级成功，自动重启

### 场景 5：启动时自动检查
- 状态：✅ 通过
- 备注：延迟 2 秒后自动检查

## 发现的问题
1. [问题描述]
2. [问题描述]

## 建议
1. [建议内容]
2. [建议内容]

## 总结
[测试总结]
```

---

## 下一步

测试完成后：

1. **收集反馈**
   - 记录所有问题
   - 收集用户体验反馈

2. **修复问题**
   - 优先修复 P0/P1 问题
   - 优化用户体验

3. **准备发布**
   - 更新文档
   - 准备 Release Notes
   - 创建 GitHub Release

4. **发布通知**
   - 通知用户新版本
   - 提供升级指南

---

**测试愉快！** 🎉

如有问题，请查看：
- `docs/AutoUpdate_Complete.md` - 完整功能文档
- `docs/CodeReview_Response.md` - 代码审查响应
- `logs/updater.log` - 升级日志
