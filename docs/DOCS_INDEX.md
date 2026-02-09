# ClawDesk MCP Server - 文档索引

## 📚 快速导航

### 🚀 快速开始

| 文档 | 说明 | 适合人群 |
|------|------|---------|
| [SETUP_MCP_CLIENT.md](SETUP_MCP_CLIENT.md) | 快速配置指南 | 想快速开始的用户 ⭐ |
| [README.md](../README.md) | 完整使用指南 | 所有用户 |
| [QUICKSTART.md](QUICKSTART.md) | 开发者快速开始 | 开发者 |

### 🔧 配置和安装

| 文档 | 说明 |
|------|------|
| [MCP_CLIENT_SETUP.md](MCP_CLIENT_SETUP.md) | MCP Client 详细配置指南 |
| [install.sh](../scripts/install.sh) | 一键安装脚本 |
| [scripts/README.md](../scripts/README.md) | 脚本说明 |
| [mcp-config-template.json](../configs/mcp-config-template.json) | MCP 配置模板 |
| [FIREWALL.md](FIREWALL.md) | 防火墙配置指南 |

### 📖 技术文档

| 文档 | 说明 |
|------|------|
| [MCP.md](MCP.md) | MCP 协议详细说明 ⭐ |
| [API.md](API.md) | HTTP API 文档 |
| [Dashboard.md](Dashboard.md) | Dashboard 技术文档 |
| [PROCESS_MODEL.md](PROCESS_MODEL.md) | 主程序/守护/升级器关系说明 |

### 🎯 用户指南

| 文档 | 说明 |
|------|------|
| [DASHBOARD_GUIDE.md](DASHBOARD_GUIDE.md) | Dashboard 使用指南 |
| [DASHBOARD_TEST.md](DASHBOARD_TEST.md) | Dashboard 测试文档 |

### 👨‍💻 开发者文档

| 文档 | 说明 |
|------|------|
| [RELEASE_GUIDE.md](RELEASE_GUIDE.md) | 发布指南 |
| [RELEASE_QUICKREF.md](RELEASE_QUICKREF.md) | 发布快速参考 |
| [RELEASE_OPTIONS.md](RELEASE_OPTIONS.md) | 发布方案对比 |
| [CLAUDE.md](../CLAUDE.md) | Claude Code 开发指南 |
| [AGENTS.md](../AGENTS.md) | 开发规范 |

## 🎯 按场景查找

### 我想快速开始使用

1. 下载 [ClawDeskMCP-x64.exe](https://github.com/ihugang/ClawWindowsDeskMcp/releases)
2. 阅读 [SETUP_MCP_CLIENT.md](SETUP_MCP_CLIENT.md)
3. 运行一键配置脚本

### 我想了解 MCP 协议

1. 阅读 [MCP.md](MCP.md) - 完整的协议说明

### 我想开发 MCP Client

1. 阅读 [MCP.md](MCP.md) - 了解端点和工具
2. 查看 [API.md](API.md) - HTTP API 详细说明
3. 使用 [mcp-config-template.json](../configs/mcp-config-template.json) - 配置模板

### 我想配置防火墙

1. 阅读 [FIREWALL.md](FIREWALL.md)

### 我想使用 Dashboard

1. 阅读 [DASHBOARD_GUIDE.md](DASHBOARD_GUIDE.md)

### 我想发布新版本

1. 阅读 [RELEASE_GUIDE.md](RELEASE_GUIDE.md)
2. 查看 [RELEASE_QUICKREF.md](RELEASE_QUICKREF.md)

## 📝 文档说明

### MCP 协议相关

**[MCP.md](MCP.md)** ⭐ 最重要的技术文档

包含：
- MCP 协议版本和实现状态
- 3 个 MCP 端点详细说明（initialize、tools/list、tools/call）
- 9 个可用工具的完整文档
- curl、Python、JavaScript 使用示例
- 与 Claude Desktop 的集成方法
- 错误处理和安全注意事项

**适合**：
- 想了解 MCP 协议的开发者
- 想开发 MCP Client 的开发者
- 想了解可用工具的用户

### 配置相关

**[MCP_CLIENT_SETUP.md](MCP_CLIENT_SETUP.md)** - 详细配置指南

包含：
- 自动配置和手动配置两种方式
- 本地访问和远程访问配置
- 验证配置的方法
- 故障排除
- 高级配置选项

**[SETUP_MCP_CLIENT.md](SETUP_MCP_CLIENT.md)** - 快速配置指南

包含：
- 一键配置命令
- 4 步手动配置
- 常见问题解答

### API 相关

**[API.md](API.md)** - HTTP API 文档

包含：
- 所有 HTTP 端点的详细说明
- 请求和响应示例
- 参数说明

## 🔗 外部链接

- **GitHub 仓库**: https://github.com/ihugang/ClawWindowsDeskMcp
- **下载**: https://github.com/ihugang/ClawWindowsDeskMcp/releases
- **Issues**: https://github.com/ihugang/ClawWindowsDeskMcp/issues
- **MCP 官方规范**: https://modelcontextprotocol.io/

## 💡 提示

- 标记 ⭐ 的文档是最常用或最重要的
- 所有文档都使用 Markdown 格式
- 可以在 GitHub 上直接阅读
- 建议按照场景查找相关文档

## 🆘 需要帮助？

1. 查看相关文档
2. 搜索 [Issues](https://github.com/ihugang/ClawWindowsDeskMcp/issues)
3. 提交新的 Issue
