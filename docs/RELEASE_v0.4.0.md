# ClawDesk MCP Server v0.4.0 发布说明

**发布日期**: 2026-02-04  
**版本**: 0.4.0  
**构建时间**: 2026-02-04 16:31 (x64), 16:32 (x86)

## 📦 下载

### Windows x64 (推荐)

- **文件**: `ClawDeskMCP-x64.exe`
- **大小**: 1.9 MB
- **MD5**: `2056ea9bdbd239958bbb8e2acb59136b`
- **位置**: `build/x64/ClawDeskMCP.exe`

### Windows x86

- **文件**: `ClawDeskMCP-x86.exe`
- **大小**: 2.0 MB
- **MD5**: `aff3f172860eec92803cfe8ee16ac057`
- **位置**: `build/x86/ClawDeskMCP.exe`

## 🎉 主要新功能

### 1. Settings Window（设置窗口）

完整的图形化设置界面，包含 7 个标签页：

- **Language（语言）**: 支持 9 种语言切换
    - 简体中文、繁体中文、英语、日语、韩语
    - 德语、法语、西班牙语、俄语
- **Startup（启动）**: Windows 自启动管理
    - 通过注册表控制开机自启动
    - 实时状态显示
- **API Key（API 密钥）**: API 密钥管理
    - 显示当前 API Key（脱敏）
    - 一键复制完整密钥
    - 重新生成密钥
- **Server（服务器）**: 服务器配置
    - 端口设置（1024-65535）
    - 自动端口选择
    - 监听地址配置
- **Security（安全）**: 安全设置
    - Bearer Token 管理
    - 白名单编辑（目录、应用、命令）
    - 高风险操作确认开关
- **Appearance（外观）**: 界面设置
    - Dashboard 自动显示
    - Dashboard 置顶
    - 托盘图标样式
    - 日志保留天数
- **About（关于）**: 系统信息
    - 版本信息
    - 许可证类型
    - 系统信息（OS、架构）

### 2. LocalizationManager（本地化管理器）

强大的多语言支持系统：

- ✅ 支持 9 种语言
- ✅ 系统语言自动检测
- ✅ 运行时语言切换
- ✅ 翻译键缺失时自动回退到英语
- ✅ 硬编码英语翻译作为后备（translations.json 缺失时）
- ✅ 支持从可执行文件目录加载翻译文件

### 3. AutoStartupManager（自启动管理器）

Windows 注册表管理：

- ✅ 读取当前自启动状态
- ✅ 启用/禁用自启动
- ✅ 错误处理和权限检查
- ✅ 注册表路径：`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`

### 4. 托盘菜单本地化

所有托盘菜单项现在支持多语言：

- ✅ 菜单项文本本地化
- ✅ 托盘提示文本本地化
- ✅ 语言切换后实时更新
- ✅ 状态信息本地化（端口、监听地址、许可证）

## 🔧 改进

### 项目结构重组

```
WinAgent/
├── build/              # 构建输出（新）
│   ├── x64/           # x64 构建产物
│   └── x86/           # x86 构建产物
├── configs/            # 配置模板（新）
├── docs/               # 文档集中管理（新）
├── scripts/            # 脚本集中管理（新）
├── src/                # 源代码
├── include/            # 头文件
├── tests/              # 测试代码
├── resources/          # 资源文件
└── third_party/        # 第三方库
```

### 编译系统改进

- ✅ 添加 comctl32 库链接（Tab 控件支持）
- ✅ 添加资源文件编译（settings_window.rc）
- ✅ 修复 Unicode/ANSI API 混用问题
- ✅ 修复构建脚本路径问题
- ✅ 版本号更新为 0.4.0

### 本地化增强

- ✅ 硬编码英语翻译作为后备
- ✅ 支持多路径查找翻译文件
- ✅ 翻译文件加载失败时不会崩溃
- ✅ 更好的错误处理

## 🐛 修复

1. ✅ 修复 main.cpp 中 GetProcessList 前向声明缺失
2. ✅ 修复 license_manager 中 verifyLicenseKey const 限定符问题
3. ✅ 修复 settings_window 中 Windows API Unicode/ANSI 版本混用
4. ✅ 修复托盘图标提示文本字符编码问题
5. ✅ 修复 build.sh 脚本路径问题
6. ✅ 修复翻译文件缺失时的崩溃问题

## 📋 使用说明

### 首次运行

1. 下载对应架构的可执行文件
2. 双击运行 `ClawDeskMCP-x64.exe` 或 `ClawDeskMCP-x86.exe`
3. 首次运行会自动生成 `config.json` 和 `auth_token`
4. 系统托盘会出现图标

### 打开设置窗口

- 右键点击托盘图标
- 选择 "Settings"（设置）菜单项
- 或使用快捷键（如果配置）

### 切换语言

1. 打开设置窗口
2. 进入 "Language" 标签页
3. 从下拉菜单选择语言
4. 点击 "Apply" 或 "OK" 保存
5. 所有界面立即更新为新语言

### 配置自启动

1. 打开设置窗口
2. 进入 "Startup" 标签页
3. 勾选 "Start with Windows"
4. 点击 "Apply" 或 "OK" 保存
5. 下次开机会自动启动

## 🔄 从 v0.3.0 升级

### 配置文件兼容性

v0.4.0 完全兼容 v0.3.0 的配置文件。新增字段：

```json
{
  "language": "en",
  "auto_startup": false,
  "api_key": "",
  "server": {
    "port": 35182,
    "auto_port": true,
    "listen_address": "0.0.0.0"
  },
  "security": {
    "bearer_token": "...",
    "allowed_dirs": [...],
    "allowed_apps": {...},
    "allowed_commands": [...],
    "high_risk_confirmations": true
  },
  "appearance": {
    "dashboard_auto_show": true,
    "dashboard_always_on_top": false,
    "tray_icon_style": "normal",
    "log_retention_days": 30
  }
}
```

### 升级步骤

1. 停止旧版本服务器
2. 备份 `config.json`（可选）
3. 替换可执行文件
4. 启动新版本
5. 配置会自动迁移

## 📝 已知问题

1. **翻译不完整**: 部分语言（繁体中文、日语、韩语等）的托盘菜单翻译尚未完成，会回退到英语
2. **Settings Window 布局**: 在某些高 DPI 显示器上可能需要调整
3. **资源文件**: 远程服务器运行时，x64 版本文件可能被占用，需要先停止服务器

## 🔮 下一步计划

- [ ] 完善所有语言的翻译
- [ ] 添加更多设置选项
- [ ] 改进 Settings Window UI/UX
- [ ] 添加主题支持
- [ ] 实现配置导入/导出

## 📞 支持

如有问题或建议，请：

- 查看文档：`docs/` 目录
- 查看 CHANGELOG.md
- 提交 Issue

---

**构建信息**:

- 编译器: MinGW-w64 13.0.0
- C++ 标准: C++17
- 构建类型: Release
- 优化级别: -O3
- 静态链接: 是
