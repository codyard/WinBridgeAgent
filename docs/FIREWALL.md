# Windows 防火墙配置指南

ClawDesk MCP Server 在监听 `0.0.0.0` 时需要配置 Windows 防火墙以允许网络访问。

## 自动配置（推荐）✨

程序首次运行时会自动处理防火墙配置：

1. **检测规则**：程序启动时自动检查是否已存在防火墙规则
2. **询问用户**：如果规则不存在，弹出对话框询问是否添加
3. **自动添加**：用户确认后，程序会请求管理员权限并自动添加规则

### 对话框示例

```
ClawDesk MCP Server needs to add a Windows Firewall rule to allow network access.

This requires administrator privileges.

Click OK to add the firewall rule, or Cancel to continue without it.

[OK] [Cancel]
```

- 点击 **OK**：程序会请求管理员权限（UAC 提示），然后自动添加防火墙规则
- 点击 **Cancel**：跳过自动配置，需要手动添加规则

## 手动配置

如果自动配置失败或被跳过，可以手动添加防火墙规则：

### 方法 1：使用命令行（推荐）

以管理员身份打开命令提示符（cmd）或 PowerShell，执行：

```cmd
netsh advfirewall firewall add rule name="ClawDesk MCP Server" dir=in action=allow program="C:\path\to\ClawDeskMCP.exe" enable=yes profile=any
```

**注意**：将 `C:\path\to\ClawDeskMCP.exe` 替换为实际的程序路径。

### 方法 2：使用图形界面

1. 按 `Win + R`，输入 `wf.msc`，按回车
2. 在左侧选择"入站规则"
3. 在右侧点击"新建规则..."
4. 选择"程序"，点击"下一步"
5. 选择"此程序路径"，浏览到 `ClawDeskMCP.exe`
6. 选择"允许连接"
7. 勾选所有配置文件（域、专用、公用）
8. 输入名称："ClawDesk MCP Server"
9. 点击"完成"

## 检查防火墙规则

### 使用命令行

```cmd
netsh advfirewall firewall show rule name="ClawDesk MCP Server"
```

如果规则存在，会显示规则详情：

```
规则名称:                             ClawDesk MCP Server
----------------------------------------------------------------------
已启用:                               是
方向:                                 入站
配置文件:                             域,专用,公用
分组:
LocalIP:                              任何
RemoteIP:                             任何
协议:                                 任何
操作:                                 允许
```

### 使用图形界面

1. 打开 Windows Defender 防火墙高级设置（`wf.msc`）
2. 选择"入站规则"
3. 在列表中查找"ClawDesk MCP Server"

## 删除防火墙规则

如果需要删除规则：

### 使用命令行

```cmd
netsh advfirewall firewall delete rule name="ClawDesk MCP Server"
```

### 使用图形界面

1. 打开 Windows Defender 防火墙高级设置（`wf.msc`）
2. 选择"入站规则"
3. 找到"ClawDesk MCP Server"
4. 右键点击，选择"删除"

## 测试网络访问

添加防火墙规则后，可以从局域网内的其他设备测试访问：

```bash
# 从另一台设备测试
curl http://192.168.x.x:35182/health

# 应该返回
{"status":"ok"}
```

## 常见问题

### Q: 为什么需要管理员权限？

A: 修改 Windows 防火墙规则需要管理员权限。程序使用 `runas` 动词请求 UAC 提升权限。

### Q: 如果不添加防火墙规则会怎样？

A: 程序仍然可以运行，但局域网内的其他设备无法访问服务器。只能通过 `localhost` 或 `127.0.0.1` 本地访问。

### Q: 监听 127.0.0.1 需要防火墙规则吗？

A: 不需要。只有监听 `0.0.0.0` 时才需要防火墙规则，因为这会允许网络访问。

### Q: 如何切换监听地址？

A: 右键点击托盘图标，选择"Toggle Listen Address"，然后重启服务器。

### Q: 防火墙规则是否安全？

A: 规则只允许入站连接到 ClawDeskMCP.exe 程序。这是标准的防火墙配置方式，与其他网络应用程序（如 Web 服务器）相同。

## 安全建议

1. **仅在可信网络中使用**：建议仅在家庭或办公室局域网中启用网络访问
2. **使用强认证**：未来版本将实现 Bearer Token 认证
3. **定期检查日志**：查看 `logs/audit-*.log` 了解访问记录
4. **限制白名单**：在 `config.json` 中严格限制 `allowed_dirs`、`allowed_apps` 和 `allowed_commands`

## 相关文档

- [README.md](README.md) - 主要文档
- [config.json](config.template.json) - 配置文件模板
