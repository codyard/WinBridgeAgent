# 更新日志

所有重要的项目更改都将记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
并且本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.8.2] - 2026-02-06

### 修复

- ✅ **设置对话框存储问题**
  - 修复设置保存时未正确同步所有控件状态的问题
  - 新增 `syncControlsToSettings()` 方法统一处理所有标签页的设置同步
  - 确保语言、启动、API Key、服务器、安全和外观设置正确保存

### 改进

- ✅ 优化设置同步逻辑
  - 统一 "应用" 和 "确定" 按钮的设置保存流程
  - 改进各标签页控件状态到配置的同步机制
  - 增强设置持久化的可靠性

### 技术细节

- 重构 `onApplyClicked()` 和 `onOkClicked()` 方法
- 从 `syncSecurityListsFromControls()` 迁移到 `syncControlsToSettings()`
- 新方法覆盖所有 6 个标签页的设置同步：
  - 语言设置
  - 启动设置
  - API Key 设置
  - 服务器设置
  - 安全设置
  - 外观设置

## [0.8.0] - 2026-02-05

### 新增

- ✅ **自动更新功能 - Phase 4：定时检查和跳过版本**
  - 定时检查更新功能
  - 跳过版本功能
  - 智能更新调度

### Phase 4 功能详情

#### 定时检查更新
- 后台定时器线程
- 可配置检查间隔（小时）
- 自动检查不干扰用户
- 智能避免重复检查
- 线程安全实现

#### 跳过版本
- 用户可选择跳过特定版本
- 跳过的版本不再提示
- 配置持久化保存
- 可通过配置文件重置

#### 改进的更新对话框
- 三按钮选项：
  - [Yes] 下载并安装
  - [No] 访问 GitHub
  - [Cancel] 跳过此版本
- 更清晰的按钮说明

### 改进

- ✅ 完整的定时检查机制
- ✅ 用户友好的版本跳过
- ✅ 更智能的更新调度
- ✅ 线程安全的定时器
- ✅ 优雅的线程退出

### 技术细节

- 定时器线程每分钟检查一次是否需要停止
- 使用原子变量控制线程生命周期
- 程序退出时正确清理定时器线程
- 跳过版本信息保存在 config.json
- 检查更新时自动过滤已跳过的版本

### 配置说明

```json
{
  "auto_update": {
    "enabled": true,
    "check_on_startup": true,
    "update_check_interval_hours": 24,
    "skipped_version": "0.7.0",
    "update_channel": "stable",
    "verify_signature": true
  }
}
```

**配置项说明**：
- `update_check_interval_hours`: 定时检查间隔（小时），0 表示禁用
- `skipped_version`: 已跳过的版本号，为空表示不跳过任何版本

### 修复

- ✅ 修复 SHA256 文件下载方式（使用 DownloadManager）
- ✅ 修复下载进度回调线程安全问题（使用 PostMessage）
- ✅ 修复 verify_signature 配置未生效问题

## [0.7.0] - 2026-02-05

### 新增

- ✅ **自动更新功能 - Phase 3：自动升级和重启**
    - Updater.exe：外置升级器程序
    - 一键下载并安装更新
    - 自动备份旧版本
    - 自动替换可执行文件
    - 自动启动新版本
    - 升级失败自动回滚
- ✅ **升级器功能**
    - 等待主程序退出
    - 备份当前版本
    - 替换可执行文件
    - 启动新版本
    - 失败时自动回滚
    - 升级日志记录
    - 进度窗口显示
- ✅ **主程序升级流程**
    - 检查更新 → 下载 → 安装 → 重启（一键完成）
    - 下载进度实时显示
    - 用户可选择立即安装或稍后安装
    - 启动升级器并自动退出
- ✅ **用户体验优化**
    - 三按钮对话框：立即安装 / 访问 GitHub / 取消
    - 下载进度窗口（带取消按钮）
    - 安装确认对话框
    - 升级过程进度提示

### 改进

- ✅ 完整的自动升级流程
- ✅ 备份和回滚机制
- ✅ 升级器独立运行，不依赖主程序
- ✅ 升级日志记录（updater.log）
- ✅ 临时文件自动清理

### 技术细节

- Updater.exe 使用 C++ 编写，独立编译
- 使用 Windows API 进行进程管理
- 支持命令行参数传递
- 升级器等待主程序退出（最多 30 秒）
- 备份文件保留为 .backup 后缀
- 升级失败时自动恢复备份

### 修复

- ✅ 修复资产命名过于严格的问题（支持多种命名格式）
- ✅ 修复线程安全问题（使用原子变量和消息传递）

## [0.6.0] - 2026-02-05

### 新增

- ✅ **自动更新功能 - Phase 2：下载和验证**
    - DownloadManager 类：文件下载管理器
    - 支持断点续传（HTTP Range 请求）
    - SHA256 哈希验证
    - 下载进度窗口（实时显示进度、速度）
    - 磁盘空间检查
    - 自动从 GitHub 下载 SHA256 文件并验证
- ✅ **下载功能**
    - 同步和异步下载接口
    - 实时进度回调
    - 取消下载功能
    - 下载速度计算
    - 错误处理和重试
- ✅ **UpdateChecker 扩展**
    - downloadUpdate() 方法：同步下载更新
    - downloadUpdateAsync() 方法：异步下载更新
    - cancelDownload() 方法：取消下载
    - 自动获取和验证 SHA256 哈希
- ✅ **下载进度窗口**
    - 实时进度条
    - 下载速度显示
    - 状态信息显示
    - 取消按钮
    - 窗口居中显示
- ✅ **多语言支持**
    - 下载相关的翻译键（简体中文、英语）
    - 进度窗口本地化

### 改进

- ✅ 使用 cpp-httplib 实现 HTTPS 下载
- ✅ OpenSSL 集成用于 SHA256 计算
- ✅ 支持 HTTP Range 请求实现断点续传
- ✅ 下载前检查磁盘空间（预留 100MB）
- ✅ 临时文件管理（.tmp 后缀）
- ✅ 下载完成后自动重命名

### 技术细节

- DownloadManager 使用 cpp-httplib SSLClient
- SHA256 使用 OpenSSL 库计算
- 进度窗口使用 Windows Common Controls
- 支持取消下载（线程安全）
- 下载速度实时计算（基于时间窗口）

### 依赖更新

- 添加 OpenSSL 链接库（ssl, crypto）
- 添加 Windows Crypto API（crypt32）

## [0.5.0] - 2026-02-05

### 新增

- ✅ **自动更新功能 - Phase 1：基础版本检查**
    - UpdateChecker 类：GitHub API 集成和版本比较
    - 语义化版本解析和比较（支持预发布版本）
    - 托盘菜单"检查更新"功能
    - 更新通知对话框（显示版本信息和 Release Notes）
    - 自动打开 GitHub Release 页面下载
    - 配置管理扩展（更新相关配置项）
- ✅ **更新配置**
    - auto_update_enabled：启用/禁用自动更新
    - update_check_interval_hours：检查更新间隔（小时）
    - update_channel：更新通道（stable/beta）
    - github_repo：GitHub 仓库配置
    - update_verify_signature：验证代码签名开关
    - last_update_check：上次检查时间记录
    - skipped_version：跳过的版本号
- ✅ **多语言支持**
    - 更新相关的翻译键（简体中文、英语）
    - 托盘菜单本地化

### 改进

- ✅ ConfigManager 扩展支持更新配置的读写
- ✅ 版本比较支持预发布版本（beta、alpha 等）
- ✅ GitHub API 调用支持重试机制（指数退避）
- ✅ 平台架构自动检测（x64/x86/ARM64）

### 技术细节

- UpdateChecker 使用 cpp-httplib 调用 GitHub REST API
- 支持 GitHub API 速率限制处理
- 异步更新检查不阻塞主线程
- 自动选择匹配当前平台的资产文件

## [0.4.0] - 2026-02-04

### 新增

- ✅ **单实例检测机制**
    - 使用 Windows 互斥锁防止多实例运行
    - 自动向旧实例发送退出命令
    - 优雅等待旧实例退出（最多8秒）
    - 超时保护和友好提示
    - 所有退出路径的资源清理
- ✅ **Settings Window（设置窗口）**
    - 多语言支持（简体中文、繁体中文、英语、日语、韩语、德语、法语、西班牙语、俄语）
    - Windows 自启动设置（注册表管理）
    - API Key 管理（显示、复制、重新生成）
    - 服务器设置（端口、自动端口、监听地址）
    - 安全设置（Bearer Token、白名单管理、高风险操作确认）
    - 外观设置（Dashboard 自动显示、置顶、托盘图标样式、日志保留天数）
    - 关于页面（版本、许可证、系统信息）
- ✅ **LocalizationManager（本地化管理器）**
    - 支持 9 种语言的翻译资源
    - 系统语言自动检测
    - 运行时语言切换
    - 翻译键缺失时自动回退到英语
    - 硬编码英语翻译作为后备方案（translations.json 缺失时）
    - 支持从可执行文件目录加载翻译文件
- ✅ **AutoStartupManager（自启动管理器）**
    - Windows 注册表读写操作
    - 启用/禁用自启动
    - 错误处理和权限检查
- ✅ **配置文件扩展**
    - 新增 language、auto_startup、api_key 字段
    - 新增 server、security、appearance 配置节
    - 配置验证（端口范围、日志保留天数）
- ✅ **托盘菜单本地化**
    - 所有菜单项支持多语言
    - 托盘提示文本本地化
    - 语言切换后实时更新
- ✅ **截图功能增强**
    - HTTP API: `/help` 端点（别名 `/`），返回完整的 API 列表和 MCP 工具列表
    - MCP 工具: `take_screenshot_window` - 通过窗口标题捕获指定窗口截图
        - 支持精确匹配和模糊匹配（不区分大小写）
        - 自动恢复最小化窗口并置于前台
        - 使用 PrintWindow API 捕获完整内容
    - MCP 工具: `take_screenshot_region` - 捕获屏幕指定区域截图
        - 支持坐标和尺寸参数（x, y, width, height）
        - 精确控制截图范围
    - 所有截图保存为 PNG 格式，返回文件路径和元数据
- ✅ **多电脑支持**
    - 在 `/status` 响应中添加 `computer_name` 字段
    - 支持通过 MCP 配置区分多台电脑
    - 提供多电脑配置模板和详细指南
    - AI 助手可根据电脑名称选择目标
- ✅ **版本自动递增**
    - 构建脚本自动递增版本号（+0.0.1）
    - 自动更新 CMakeLists.txt
    - 显示版本变更信息

### 修复

- ✅ **语言设置持久化**
    - 语言选择后立即保存到配置文件
    - 修复语言切换后重启失效的问题
- ✅ **语言下拉框滚动条**
    - 增加下拉框高度从 120 到 200 像素
    - 添加 CBS_DISABLENOSCROLL 和 WS_VSCROLL 样式
    - 确保所有 9 种语言都可见
- ✅ **编译错误修复**
    - 修复 settings*window.h 中重复的 uiFont* 声明
    - 修复 x86 构建中的 lambda 函数调用约定问题
    - 使用 CALLBACK 函数替代 lambda 以兼容 Windows API

### 改进

- ✅ 项目结构重组
    - 所有文档移至 `docs/` 目录
    - 所有脚本移至 `scripts/` 目录
    - 配置模板移至 `configs/` 目录
    - 构建输出统一到 `build/x64/` 和 `build/x86/`
- ✅ CMakeLists.txt 更新
    - 添加 comctl32 库链接（Tab 控件支持）
    - 添加资源文件编译（settings_window.rc）
    - 版本号更新为 0.4.0
- ✅ 编译优化
    - 修复 Unicode/ANSI API 混用问题
    - 修复 const 限定符问题
    - 添加前向声明
- ✅ 本地化增强
    - 添加硬编码英语翻译作为后备
    - 支持多路径查找翻译文件
    - 翻译文件加载失败时不会崩溃

### 修复

- ✅ 修复 main.cpp 中 GetProcessList 前向声明缺失
- ✅ 修复 license_manager 中 verifyLicenseKey const 限定符问题
- ✅ 修复 settings_window 中 Windows API Unicode/ANSI 版本混用
- ✅ 修复托盘图标提示文本字符编码问题
- ✅ 修复 build.sh 脚本路径问题
- ✅ 修复翻译文件缺失时的崩溃问题
- ✅ **修复设置窗口崩溃问题**（使用 CreateWindowEx 替代 DialogBoxParam，不再依赖资源文件）
- ✅ **修复 GetSelectedText 前向声明问题**（Codex 修改后的编译错误）

## [0.3.0] - 2026-02-04

### 新增

- ✅ Settings Window（Language/Startup/API Key/Server/Security/Appearance/About）基础实现
- ✅ 本地化基础设施与自动启动注册表管理
- ✅ 配置文件扩展与原子保存

## [0.2.0] - 2026-02-03

### 新增

- ✅ **Token 认证**
    - 所有 API 端点需要 Bearer Token 认证
    - 首次启动自动生成 64 字符十六进制 Token
    - Token 保存在 `config.json` 的 `auth_token` 字段
    - Token 缺失或无效时自动重新生成
    - CORS 预检请求（OPTIONS）无需认证
    - 详见 [Authentication Guide](docs/Authentication.md)
- ✅ 系统托盘图标和菜单
- ✅ Dashboard 实时监控窗口
    - 实时日志显示（REQ/PRO/OK/ERR）
    - 自动滚动和日志限制（1000 条）
    - 清空和复制功能
    - 置顶浮动窗口
- ✅ HTTP API 服务器
    - `GET /` - API 端点列表
    - `GET /health` - 健康检查
    - `GET /status` - 服务器状态
    - `GET /exit` - 退出服务器
- ✅ 磁盘和文件操作
    - `GET /disks` - 磁盘列表（类型、容量、文件系统）
    - `GET /list?path=<path>` - 目录列表
    - `GET /read?path=<path>&start=<n>&lines=<n>&tail=<n>&count=<bool>` - 文件读取
        - 支持行范围读取
        - 支持从尾部读取
        - 支持只获取行数
    - `GET /search?path=<path>&query=<text>&case=<i|sensitive>&max=<n>` - 文件搜索
        - 大小写敏感/不敏感
        - 结果数量限制
- ✅ 剪贴板操作
    - `GET /clipboard` - 读取剪贴板（支持文本、图片、文件）
    - `PUT /clipboard` - 写入剪贴板（文本）
    - `GET /clipboard/image/<filename>` - 下载剪贴板图片
    - `GET /clipboard/file/<filename>` - 下载剪贴板文件
    - 自动检测剪贴板内容类型
    - 图片保存为 PNG 格式
    - 支持多文件复制
- ✅ 屏幕截图
    - `GET /screenshot?format=<png|jpg>` - 捕获截图
    - PNG 和 JPEG 格式支持
    - Base64 编码返回
    - GDI+ 图像处理
- ✅ ConfigManager（配置管理）
    - JSON 配置文件
    - 自动生成默认配置
    - 端口和监听地址配置
    - 白名单配置
- ✅ AuditLogger（审计日志）
    - 操作日志记录
    - 时间戳和风险等级
    - 执行结果和错误信息
- ✅ 防火墙集成

### 安全性

- 🔒 添加 Bearer Token 认证防止未授权访问
- 🔒 所有请求（除 OPTIONS）都需要 Token 验证
- 🔒 使用加密安全随机数生成器生成 Token
- 🔒 Token 验证失败返回 401 Unauthorized
    - 自动检测防火墙规则
    - 请求管理员权限添加规则
    - 用户确认机制
- ✅ CORS 支持
    - 跨域资源共享
    - OPTIONS 预检请求
- ✅ 监听地址切换
    - 0.0.0.0（网络访问）
    - 127.0.0.1（本地访问）
- ✅ 交叉编译环境
    - macOS 开发环境
    - MinGW-w64 交叉编译
    - x64 和 x86 支持

### 修复

- ✅ Dashboard 窗口退出问题
    - 使用 DestroyWindow 强制关闭
    - 使用 PostMessage 异步关闭主窗口
    - 修复退出时的死锁问题
- ✅ Dashboard 窗口可见性问题
    - 添加错误检查
    - 显式窗口定位
    - SetForegroundWindow 确保窗口显示
- ✅ CORS 预检请求处理
    - 添加 OPTIONS 方法支持
    - 返回正确的 CORS 头

### 改进

- ✅ Dashboard 日志记录
    - 移除 isVisible() 检查
    - 确保所有日志都被记录
    - 即使窗口未显示也记录日志
- ✅ 文件操作
    - URL 编码/解码支持
    - JSON 字符转义
    - 10MB 文件大小限制
- ✅ 错误处理
    - HTTP 状态码
    - 错误消息
    - Dashboard 错误日志

### 文档

- ✅ README.md - 项目说明和快速开始
- ✅ docs/API.md - 完整的 API 文档
- ✅ docs/Dashboard.md - Dashboard 使用指南
- ✅ docs/ConfigManager.md - 配置管理文档
- ✅ docs/AuditLogger.md - 审计日志文档
- ✅ docs/FEATURES.md - 功能列表和统计
- ✅ CHANGELOG.md - 更新日志（本文档）
- ✅ docs/DASHBOARD_GUIDE.md - Dashboard 快速指南
- ✅ docs/DASHBOARD_TEST.md - Dashboard 测试指南
- ✅ docs/FIREWALL.md - 防火墙配置指南

### 测试

- ✅ scripts/test_dashboard.sh - Dashboard 测试脚本
- ✅ scripts/test_file_ops.sh - 文件操作测试脚本
- ✅ scripts/test_clipboard.sh - 剪贴板测试脚本
- ✅ scripts/test_screenshot.sh - 截图测试脚本
- ✅ scripts/test_all_features.sh - 综合测试脚本

## [0.1.0] - 2026-01-XX

### 新增

- ✅ 基础项目结构
- ✅ CMake 构建系统
- ✅ MinGW-w64 工具链配置
- ✅ 基础 HTTP 服务器
- ✅ 简单的健康检查端点

## [未发布]

### 计划中 (v0.3.0)

- ⏳ MCP 协议实现
    - 工具注册
    - 工具调用
    - 工具响应
- ⏳ 窗口管理
    - 窗口列表
    - 窗口激活
    - 窗口最小化/最大化
- ⏳ 进程管理
    - 进程列表
    - 进程启动
    - 进程终止
- ⏳ 命令执行
    - 执行系统命令
    - 捕获输出
    - 错误处理
- ⏳ 文件写入操作
    - 创建文件
    - 写入内容
    - 追加内容
    - 删除文件

### 计划中 (v0.4.0)

- ⏳ Bearer Token 认证
    - Token 生成
    - Token 验证
    - Token 刷新
- ⏳ 许可证管理
    - 许可证验证
    - 功能限制
    - 到期检查
- ⏳ 配额系统
    - 调用次数限制
    - 截图次数限制
    - 配额重置
- ⏳ 使用统计
    - 今日调用统计
    - 历史记录
    - 导出报告

### 计划中 (v0.5.0)

- ⏳ 系统信息
    - CPU 信息
    - 内存信息
    - 磁盘信息
    - 网络信息
- ⏳ 网络操作
    - 网络连接列表
    - 端口监听
    - 网络测试
- ⏳ 注册表操作
    - 读取注册表
    - 写入注册表
    - 删除注册表项
- ⏳ 服务管理
    - 服务列表
    - 启动/停止服务
    - 服务状态查询

## 版本说明

### 版本号格式

- 主版本号：重大架构变更或不兼容的 API 变更
- 次版本号：新功能添加，向后兼容
- 修订号：Bug 修复和小改进

### 发布周期

- 主版本：按需发布
- 次版本：每月发布
- 修订号：每周发布（如有必要）

## 贡献者

感谢所有为本项目做出贡献的开发者！

## 反馈

如果您发现任何问题或有功能建议，请：

1. 查看现有 Issues
2. 创建新的 Issue
3. 提供详细的描述和复现步骤
4. 包含系统信息和日志

## 许可证

Copyright © 2026 ClawDesk
