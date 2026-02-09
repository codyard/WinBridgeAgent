# ClawDesk MCP Server - 快速配置指南

## 🚀 一键配置 MCP Client

### 方式 1：使用自动配置脚本（推荐）

在你的 Mac 或 Linux 上运行：

```bash
curl -fsSL https://raw.githubusercontent.com/ihugang/ClawWindowsDeskMcp/main/scripts/install.sh | bash
```

这个脚本会：
1. ✅ 检测你的操作系统
2. ✅ 验证服务器连接
3. ✅ 自动配置 Claude Desktop
4. ✅ 测试认证

### 方式 2：手动配置

#### 步骤 1: 下载并运行 ClawDesk MCP Server

1. 访问 [Releases](https://github.com/ihugang/ClawWindowsDeskMcp/releases)
2. 下载 `ClawDeskMCP-x64.exe`（或其他版本）
3. 双击运行，程序会在系统托盘显示图标

#### 步骤 2: 获取配置信息

打开 `config.json`（与可执行文件在同一目录），找到：

```json
{
  "auth_token": "abc123...",  // 复制这个 token
  ...
}
```

#### 步骤 3: 配置 Claude Desktop

编辑 Claude Desktop 配置文件：

**Windows**: `%APPDATA%\Claude\claude_desktop_config.json`
**macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`
**Linux**: `~/.config/Claude/claude_desktop_config.json`

添加以下内容：

```json
{
  "mcpServers": {
    "clawdesk": {
      "url": "http://localhost:35182",
      "transport": "http",
      "headers": {
        "Authorization": "Bearer YOUR_AUTH_TOKEN_HERE"
      }
    }
  }
}
```

将 `YOUR_AUTH_TOKEN_HERE` 替换为步骤 2 中的 token。

#### 步骤 4: 重启 Claude Desktop

配置完成！现在你可以在 Claude Desktop 中使用 ClawDesk 的功能了。

## 📖 详细文档

- [MCP Client 配置指南](MCP_CLIENT_SETUP.md) - 完整配置说明
- [MCP 协议文档](docs/MCP.md) - MCP 协议详细说明 ⭐
- [README.md](README.md) - 使用指南
- [FIREWALL.md](FIREWALL.md) - 防火墙配置

### MCP 协议说明

想了解 MCP 协议的详细信息？查看：

📖 **[docs/MCP.md](docs/MCP.md)** - 包含：
- MCP 协议端点定义
- 9 个可用工具的详细说明
- curl、Python、JavaScript 使用示例
- 错误处理和安全注意事项

## 🔗 快速链接

- **下载**: https://github.com/ihugang/ClawWindowsDeskMcp/releases
- **配置模板**: [../configs/mcp-config-template.json](../configs/mcp-config-template.json)
- **安装脚本**: [../scripts/install.sh](../scripts/install.sh)

## ❓ 常见问题

### Q: 如何验证配置是否成功？

A: 在浏览器中访问 `http://localhost:35182/health`，应该返回 `{"status":"ok"}`

### Q: 如何从其他设备访问？

A: 将 `localhost` 替换为 Windows 机器的 IP 地址，如 `http://192.168.1.100:35182`

### Q: Token 在哪里？

A: 在 ClawDesk MCP Server 的 `config.json` 文件中，字段名为 `auth_token`

## 🆘 需要帮助？

- 查看 [Issues](https://github.com/ihugang/ClawWindowsDeskMcp/issues)
- 提交新的 Issue
- 查看详细文档
