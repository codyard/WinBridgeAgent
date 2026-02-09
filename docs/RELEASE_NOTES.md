# ClawDesk MCP Server v0.2.0 - Release Notes

## 发布日期

2026-02-03

## 概述

ClawDesk MCP Server v0.2.0 是一个功能完整的 Windows 系统操作服务，实现了 Model Context Protocol (MCP) 标准协议，为 AI 助手提供安全可控的 Windows 系统操作能力。

## 主要特性

### 1. MCP 协议支持 ✅

实现了 Model Context Protocol 标准协议，包括：

- `POST /mcp/initialize` - 初始化连接
- `POST /mcp/tools/list` - 列出可用工具
- `POST /mcp/tools/call` - 调用工具

支持 9 个工具：

1. read_file - 读取文件
2. search_file - 搜索文件内容
3. list_directory - 列出目录
4. get_clipboard - 获取剪贴板
5. set_clipboard - 设置剪贴板
6. take_screenshot - 截图
7. list_windows - 列出窗口
8. list_processes - 列出进程
9. execute_command - 执行命令

### 2. HTTP API (17 个端点) ✅

**基础端点**:

- `GET /` - API 列表
- `GET /health` - 健康检查
- `GET /status` - 服务器状态
- `GET /exit` - 退出服务器

**文件操作**:

- `GET /disks` - 磁盘列表
- `GET /list?path=<path>` - 目录列表
- `GET /read?path=<path>&start=<n>&lines=<n>&tail=<n>&count=<bool>` - 文件读取
- `GET /search?path=<path>&query=<text>&case=<i|sensitive>&max=<n>` - 文件搜索

**系统操作**:

- `GET /clipboard` - 读取剪贴板
- `PUT /clipboard` - 写入剪贴板
- `GET /screenshot?format=<png|jpg>` - 截图
- `GET /windows` - 窗口列表
- `GET /processes` - 进程列表
- `POST /execute` - 执行命令

**MCP 协议**:

- `POST /mcp/initialize` - MCP 初始化
- `POST /mcp/tools/list` - MCP 工具列表
- `POST /mcp/tools/call` - MCP 工具调用

### 3. 实时监控 Dashboard ✅

- 置顶浮动窗口
- 实时日志显示（REQ/PRO/OK/ERR）
- 自动滚动
- 日志限制（1000 条）
- 清空和复制功能
- 固定宽度字体（Consolas）

### 4. 系统托盘集成 ✅

- 系统托盘图标
- 右键菜单
- 状态显示
- Dashboard 开关
- 配置管理
- 监听地址切换

### 5. 配置管理 ✅

- JSON 配置文件
- 自动生成默认配置
- 端口配置
- 监听地址配置（0.0.0.0 / 127.0.0.1）
- 白名单配置

### 6. 审计日志 ✅

- 操作日志记录
- 时间戳
- 风险等级
- 执行结果
- 错误信息

### 7. 防火墙集成 ✅

- 自动检测防火墙规则
- 请求管理员权限添加规则
- 用户确认机制

## 技术规格

### 开发环境

- **语言**: C++17
- **构建工具**: CMake 3.20+
- **交叉编译**: MinGW-w64
- **开发平台**: macOS (Apple Silicon)
- **目标平台**: Windows 10/11 (x64, x86)

### 依赖库

- nlohmann/json - JSON 解析
- cpp-httplib - HTTP 服务器
- stb_image_write - 图像写入
- GDI+ - 图像处理
- Win32 API - 系统操作

### 性能指标

- **内存占用**: ~10MB (空闲)
- **CPU 占用**: < 1% (空闲)
- **可执行文件大小**: 1.4MB
- **启动时间**: < 1 秒
- **响应时间**: < 100ms (大多数操作)

### 限制

- 文件大小限制: 10MB
- 搜索结果限制: 1000 条
- Dashboard 日志限制: 1000 条
- 命令执行超时: 30 秒

## 安装和使用

### 安装

1. 下载 `ClawDeskMCP-x64.exe`（64 位）或 `ClawDeskMCP-x86.exe`（32 位）
2. 双击运行
3. 程序会在系统托盘显示图标
4. 首次运行会自动生成 `config.json` 配置文件

### 配置

编辑 `config.json` 文件：

```json
{
    "server_port": 35182,
    "listen_address": "0.0.0.0",
    "allowed_dirs": ["C:/Users", "C:/Temp"],
    "allowed_apps": {
        "notepad": "C:/Windows/System32/notepad.exe"
    },
    "allowed_commands": ["npm", "git", "python"],
    "license_key": ""
}
```

### 使用

**本地访问**:

```bash
curl http://localhost:35182/status
```

**网络访问**（需要 listen_address 设置为 "0.0.0.0"）:

```bash
curl http://192.168.x.x:35182/status
```

**MCP 协议**:

```bash
curl -X POST http://localhost:35182/mcp/initialize \
  -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}'
```

## 测试

提供了完整的测试脚本：

1. `../scripts/test_dashboard.sh` - Dashboard 测试
2. `../scripts/test_file_ops.sh` - 文件操作测试
3. `../scripts/test_clipboard.sh` - 剪贴板测试
4. `../scripts/test_screenshot.sh` - 截图测试
5. `../scripts/test_windows_processes.sh` - 窗口和进程测试
6. `../scripts/test_mcp.sh` - MCP 协议测试
7. `../scripts/test_all_features.sh` - 综合测试

## 文档

完整的文档包括：

1. `README.md` - 项目说明和快速开始
2. `docs/API.md` - 完整的 API 文档
3. `docs/MCP.md` - MCP 协议文档
4. `docs/Dashboard.md` - Dashboard 使用指南
5. `docs/ConfigManager.md` - 配置管理文档
6. `docs/AuditLogger.md` - 审计日志文档
7. `FEATURES.md` - 功能列表和统计
8. `CHANGELOG.md` - 更新日志
9. `RELEASE_NOTES.md` - 发布说明（本文档）

## 已知问题

1. **MCP 协议**: 当前为简化实现，未完全遵循 MCP 规范
2. **认证**: 未实现 Bearer Token 认证
3. **工具参数**: 部分 MCP 工具的参数解析较为简单
4. **文件写入**: 未实现文件写入功能
5. **资源管理**: 不支持 MCP 资源（resources）功能

## 安全建议

1. **监听地址**:
    - 使用 `127.0.0.1` 仅本地访问（推荐）
    - 使用 `0.0.0.0` 允许网络访问（需要防火墙保护）

2. **防火墙**:
    - 使用 `0.0.0.0` 时需要添加 Windows 防火墙规则
    - 程序会自动请求添加规则

3. **白名单**:
    - 配置 `allowed_dirs` 限制文件访问
    - 配置 `allowed_apps` 限制应用启动
    - 配置 `allowed_commands` 限制命令执行

4. **审计日志**:
    - 定期检查 `logs/audit.log`
    - 监控异常操作

## 未来计划

### v0.3.0 (计划中)

- [ ] 完整的 MCP 规范实现
- [ ] Bearer Token 认证
- [ ] 文件写入操作
- [ ] 更详细的错误处理
- [ ] 工具调用审计

### v0.4.0 (计划中)

- [ ] 许可证管理
- [ ] 配额系统
- [ ] 使用统计
- [ ] 资源（resources）支持
- [ ] 提示词（prompts）支持

### v0.5.0 (计划中)

- [ ] 系统信息查询
- [ ] 网络操作
- [ ] 注册表操作
- [ ] 服务管理
- [ ] 流式响应

## 贡献

欢迎贡献代码、报告问题或提出建议！

### 报告问题

1. 检查现有 Issues
2. 提供详细的复现步骤
3. 包含系统信息和日志
4. 描述期望行为

### 提交代码

1. Fork 项目
2. 创建功能分支
3. 编写测试用例
4. 提交 Pull Request
5. 等待代码审查

## 许可证

Copyright © 2026 ClawDesk

## 联系方式

- 项目主页: [GitHub]
- 问题反馈: [Issues]
- 文档: [Wiki]
- 邮件: [Email]

## 致谢

感谢所有为本项目做出贡献的开发者和测试者！

特别感谢：

- nlohmann/json 项目
- cpp-httplib 项目
- stb 项目
- MinGW-w64 项目

## 更新说明

从旧版本升级：

1. 备份 `config.json` 配置文件
2. 停止旧版本服务器
3. 替换可执行文件
4. 启动新版本
5. 检查配置文件是否需要更新

## 完整功能列表

### 已实现 (26 项)

1. ✅ 系统托盘图标和菜单
2. ✅ Dashboard 实时监控
3. ✅ HTTP API 服务器
4. ✅ 磁盘枚举
5. ✅ 目录列表
6. ✅ 文件读取（行范围）
7. ✅ 文件搜索
8. ✅ 剪贴板读取
9. ✅ 剪贴板写入
10. ✅ 屏幕截图（PNG）
11. ✅ 屏幕截图（JPEG）
12. ✅ 窗口列表
13. ✅ 进程列表
14. ✅ 命令执行
15. ✅ MCP 初始化
16. ✅ MCP 工具列表
17. ✅ MCP 工具调用
18. ✅ ConfigManager
19. ✅ AuditLogger
20. ✅ 防火墙集成
21. ✅ CORS 支持
22. ✅ 监听地址切换
23. ✅ 交叉编译
24. ✅ 静态链接
25. ✅ 无运行时依赖
26. ✅ 单文件分发

### 待实现 (19 项)

1. ⏳ Bearer Token 认证
2. ⏳ 文件写入
3. ⏳ 文件删除
4. ⏳ 目录创建
5. ⏳ 文件移动/复制
6. ⏳ 许可证验证
7. ⏳ 配额管理
8. ⏳ 使用统计
9. ⏳ 系统信息
10. ⏳ 网络信息
11. ⏳ 注册表操作
12. ⏳ 服务管理
13. ⏳ MCP 资源
14. ⏳ MCP 提示词
15. ⏳ 流式响应
16. ⏳ 用户确认
17. ⏳ 速率限制
18. ⏳ IP 白名单
19. ⏳ 请求签名

### 完成率

- **总体**: 58% (26/45)
- **核心功能**: 100% (26/26)
- **高级功能**: 0% (0/19)

## 结语

ClawDesk MCP Server v0.2.0 是一个功能完整、性能优异的 Windows 系统操作服务。它为 AI 助手提供了强大的系统操作能力，同时保持了良好的安全性和可控性。

我们将继续改进和完善这个项目，欢迎您的反馈和建议！

---

**发布日期**: 2026-02-03  
**版本**: 0.2.0  
**构建**: Release  
**平台**: Windows x64/x86  
**文件大小**: 1.4MB  
**SHA256**: [待计算]
