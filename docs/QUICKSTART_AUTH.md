# 快速开始 - Token 认证

## 1. 启动服务器

运行 `ClawDeskMCP-x64.exe`，服务器会自动生成 `config.json` 文件。

## 2. 获取 Token

打开 `config.json` 文件，找到 `auth_token` 字段：

```json
{
  "auth_token": "a1b2c3d4e5f6789012345678901234567890abcdefabcdefabcdefabcdefabcd",
  "server_port": 35182,
  ...
}
```

复制这个 64 字符的十六进制字符串。

## 3. 使用 Token

### 方法 1：curl 命令

```bash
# 设置 Token 变量
TOKEN="your-token-here"

# 测试连接
curl -H "Authorization: Bearer $TOKEN" http://localhost:35182/status
```

### 方法 2：PowerShell

```powershell
# 设置 Token
$TOKEN = "your-token-here"
$headers = @{ "Authorization" = "Bearer $TOKEN" }

# 测试连接
Invoke-RestMethod -Uri "http://localhost:35182/status" -Headers $headers
```

### 方法 3：Python

```python
import requests

TOKEN = "your-token-here"
headers = {"Authorization": f"Bearer {TOKEN}"}

response = requests.get("http://localhost:35182/status", headers=headers)
print(response.json())
```

### 方法 4：JavaScript

```javascript
const TOKEN = "your-token-here";
const headers = { Authorization: `Bearer ${TOKEN}` };

fetch("http://localhost:35182/status", { headers })
    .then((res) => res.json())
    .then((data) => console.log(data));
```

## 4. 常见问题

### Q: 请求返回 401 Unauthorized？

**A:** 检查以下几点：

1. Token 是否正确（从 config.json 复制）
2. Authorization 头格式是否正确（`Bearer <token>`，注意空格）
3. Token 前后是否有多余的空格或换行符

### Q: 如何更换 Token？

**A:** 两种方法：

1. 手动编辑 `config.json`，修改 `auth_token` 字段
2. 删除 `config.json`，重启服务器会自动生成新 Token

### Q: Token 会过期吗？

**A:** 不会。Token 永久有效，直到你手动更换。

### Q: 可以禁用 Token 认证吗？

**A:** 不可以。从 v0.2.0 开始，Token 认证是强制的安全特性。

## 5. 测试脚本

运行测试脚本验证认证是否正常工作：

```bash
../scripts/test_auth.sh
```

脚本会提示你输入 Token，然后自动测试：

- ✓ 无 Token 请求（应返回 401）
- ✓ 无效 Token 请求（应返回 401）
- ✓ 有效 Token 请求（应返回 200）
- ✓ CORS 预检请求（应返回 200，无需 Token）

## 6. 完整示例

```bash
# 1. 设置 Token
TOKEN="a1b2c3d4e5f6789012345678901234567890abcdefabcdefabcdefabcdefabcd"

# 2. 测试各个端点
curl -H "Authorization: Bearer $TOKEN" http://localhost:35182/status
curl -H "Authorization: Bearer $TOKEN" http://localhost:35182/disks
curl -H "Authorization: Bearer $TOKEN" http://localhost:35182/clipboard
curl -H "Authorization: Bearer $TOKEN" http://localhost:35182/windows

# 3. POST 请求示例
curl -X POST \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"command":"echo Hello"}' \
  http://localhost:35182/execute
```

## 7. 更多信息

- [完整认证指南](docs/Authentication.md)
- [API 文档](docs/API.md)
- [MCP 协议文档](docs/MCP.md)
- [README](README.md)
