# MCP Client 配置指南

本文档说明如何配置 MCP Client（如 Claude Desktop、OpenClaw）连接到 ClawDesk MCP Server。

## 快速配置

### 方式 1：自动配置（推荐）

我们提供了一个自动配置脚本，只需一个命令：

```bash
# 下载并运行配置脚本
curl -fsSL https://raw.githubusercontent.com/ihugang/ClawWindowsDeskMcp/main/scripts/install.sh | bash
```

这个脚本会：
1. ✅ 下载最新版本的 ClawDesk MCP Server
2. ✅ 启动服务器并生成配置文件
3. ✅ 自动配置 MCP Client（Claude Desktop）
4. ✅ 显示配置信息

### 方式 2：手动配置

#### 步骤 1: 下载并运行 ClawDesk MCP Server

1. 访问 [Releases 页面](https://github.com/ihugang/ClawWindowsDeskMcp/releases)
2. 下载适合你系统的版本：
   - `ClawDeskMCP-x64.exe` (64位 Windows)
   - `ClawDeskMCP-x86.exe` (32位 Windows)
   - `ClawDeskMCP-arm64.exe` (ARM64 Windows)
3. 双击运行，程序会在系统托盘显示图标
4. 首次运行会自动生成 `config.json` 配置文件

#### 步骤 2: 获取认证 Token

打开 `config.json` 文件（与可执行文件在同一目录），找到 `auth_token` 字段：

```json
{
  "auth_token": "randomly-generated-token-here",
  ...
}
```

复制这个 token，稍后会用到。

#### 步骤 3: 配置 MCP Client

##### 对于 Claude Desktop

1. 找到 Claude Desktop 的配置文件：
   - **Windows**: `%APPDATA%\Claude\claude_desktop_config.json`
   - **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`
   - **Linux**: `~/.config/Claude/claude_desktop_config.json`

2. 编辑配置文件，添加 ClawDesk MCP Server：

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

将 `YOUR_AUTH_TOKEN_HERE` 替换为步骤 2 中复制的 token。

3. 重启 Claude Desktop

##### 对于其他 MCP Client

参考你的 MCP Client 文档，配置以下信息：

- **服务器地址**: `http://localhost:35182`
- **传输协议**: HTTP
- **认证方式**: Bearer Token
- **Token**: 从 `config.json` 中获取

## 配置模板

我们提供了一个配置模板文件 `../configs/mcp-config-template.json`，你可以：

1. 下载模板：
   ```bash
   curl -O https://raw.githubusercontent.com/ihugang/ClawWindowsDeskMcp/main/configs/mcp-config-template.json
   ```

2. 编辑模板，替换 `YOUR_AUTH_TOKEN_HERE`

3. 合并到你的 MCP Client 配置文件中

## 网络访问配置

### 本地访问（默认）

默认情况下，ClawDesk MCP Server 监听 `0.0.0.0:35182`，允许局域网访问。

如果你只想本地访问，可以修改 `config.json`：

```json
{
  "listen_address": "127.0.0.1",
  ...
}
```

### 远程访问

如果你想从其他设备访问（如从 Mac 访问 Windows 上的服务器）：

1. 确保 `config.json` 中 `listen_address` 设置为 `"0.0.0.0"`

2. 配置防火墙允许端口 35182（参考 [FIREWALL.md](FIREWALL.md)）

3. 在 MCP Client 配置中使用服务器的 IP 地址：
   ```json
   {
     "url": "http://192.168.1.100:35182",
     ...
   }
   ```

## 验证配置

### 方法 1: 使用 curl 测试

```bash
# 健康检查
curl http://localhost:35182/health

# 应该返回
{"status":"ok"}
```

### 方法 2: 使用浏览器

访问 `http://localhost:35182/status`，应该看到服务器状态信息。

### 方法 3: 在 MCP Client 中测试

在 Claude Desktop 中，尝试使用 ClawDesk 的功能，如：

```
请帮我读取剪贴板内容
```

如果配置正确，Claude 会调用 ClawDesk MCP Server 的 `clipboard_read` 工具。

## 故障排除

### 问题 1: 连接被拒绝

**原因**: ClawDesk MCP Server 未运行

**解决**:
1. 检查系统托盘是否有 ClawDesk 图标
2. 如果没有，双击运行 `ClawDeskMCP-x64.exe`

### 问题 2: 认证失败

**原因**: Token 不正确或未配置

**解决**:
1. 检查 `config.json` 中的 `auth_token`
2. 确保 MCP Client 配置中的 token 与之匹配
3. 注意不要有多余的空格或换行

### 问题 3: 端口被占用

**原因**: 端口 35182 已被其他程序占用

**解决**:
1. 修改 `config.json` 中的 `server_port`
2. 或启用 `auto_port` 让服务器自动选择端口
3. 查看 `runtime.json` 获取实际端口号
4. 更新 MCP Client 配置中的端口

### 问题 4: 防火墙阻止

**原因**: Windows 防火墙阻止了连接

**解决**:
1. 参考 [FIREWALL.md](FIREWALL.md) 配置防火墙
2. 或在防火墙提示时选择"允许访问"

## 高级配置

### 自定义端口

编辑 `config.json`：

```json
{
  "server_port": 8080,
  "auto_port": false,
  ...
}
```

然后在 MCP Client 配置中使用新端口：

```json
{
  "url": "http://localhost:8080",
  ...
}
```

### 配置白名单

编辑 `config.json`，添加允许访问的目录：

```json
{
  "allowed_dirs": [
    "C:/Users/YourName/Documents",
    "C:/Users/YourName/Downloads"
  ],
  ...
}
```

### 查看使用统计

右键点击系统托盘图标，选择 "Usage Statistics" 查看今日使用情况。

## 配置示例

### 完整的 Claude Desktop 配置示例

```json
{
  "mcpServers": {
    "clawdesk": {
      "url": "http://localhost:35182",
      "transport": "http",
      "headers": {
        "Authorization": "Bearer abc123def456ghi789jkl012mno345pqr678stu901vwx234yz"
      },
      "description": "ClawDesk MCP Server - Windows 本地能力服务"
    },
    "other-server": {
      "command": "node",
      "args": ["path/to/other-server.js"]
    }
  }
}
```

### 多服务器配置

你可以同时配置多个 MCP Server：

```json
{
  "mcpServers": {
    "clawdesk": {
      "url": "http://localhost:35182",
      "transport": "http",
      "headers": {
        "Authorization": "Bearer YOUR_TOKEN_1"
      }
    },
    "clawdesk-remote": {
      "url": "http://192.168.1.100:35182",
      "transport": "http",
      "headers": {
        "Authorization": "Bearer YOUR_TOKEN_2"
      },
      "description": "远程 Windows 机器"
    }
  }
}
```

## 安全建议

1. **不要分享你的 Auth Token**
   - Token 相当于密码，不要泄露给他人
   - 如果泄露，删除 `config.json` 重新生成

2. **限制网络访问**
   - 如果只在本地使用，设置 `listen_address` 为 `"127.0.0.1"`
   - 如果需要远程访问，使用防火墙限制访问来源

3. **配置白名单**
   - 只添加必要的目录到 `allowed_dirs`
   - 只添加必要的应用到 `allowed_apps`

4. **定期检查日志**
   - 查看 `logs/audit-*.log` 了解服务器的使用情况
   - 发现异常及时处理

## 相关文档

- [README.md](README.md) - 完整使用指南
- [FIREWALL.md](FIREWALL.md) - 防火墙配置指南
- [docs/API.md](docs/API.md) - HTTP API 文档
- [docs/MCP.md](docs/MCP.md) - MCP 协议详细说明 ⭐
- [docs/MCP-Requirements.md](docs/MCP-Requirements.md) - MCP 协议需求规范

## MCP 协议说明

ClawDesk MCP Server 实现了标准的 MCP 协议。详细的协议说明、端点定义、工具列表和使用示例，请参考：

📖 **[docs/MCP.md](docs/MCP.md)** - MCP 协议完整文档

该文档包含：
- MCP 协议端点（initialize、tools/list、tools/call）
- 9 个可用工具的详细说明
- curl、Python、JavaScript 使用示例
- 与 Claude Desktop 的集成方法

## 需要帮助？

如有问题，请：
1. 查看 [Issues](https://github.com/ihugang/ClawWindowsDeskMcp/issues)
2. 提交新的 Issue
3. 查看详细文档

## 快速链接

- **下载**: https://github.com/ihugang/ClawWindowsDeskMcp/releases
- **文档**: https://github.com/ihugang/ClawWindowsDeskMcp
- **Issues**: https://github.com/ihugang/ClawWindowsDeskMcp/issues
