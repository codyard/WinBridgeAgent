# 剪贴板功能增强说明

## 版本信息

- **版本**: v0.2.0 (Clipboard Enhanced)
- **发布日期**: 2026-02-03
- **文件**: ClawDeskMCP-x64-clipboard-enhanced.exe

## 新增功能

### 1. 剪贴板图片支持

当剪贴板中包含图片时（如截图、复制的图片），服务器会自动保存图片到本地文件，并返回访问 URL。

**端点**: `GET /clipboard`

**响应示例**（图片）:

```json
{
    "type": "image",
    "format": "png",
    "url": "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png",
    "path": "/clipboard/image/clipboard_images/clipboard_20260203_223015.png"
}
```

**下载图片**:

```bash
# 使用返回的 URL 直接下载
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png" \
  -o downloaded_image.png
```

### 2. 剪贴板文件支持

当剪贴板中包含文件（如从资源管理器复制的文件）时，服务器会自动复制文件到本地，并返回文件列表和访问 URL。

**端点**: `GET /clipboard`

**响应示例**（文件）:

```json
{
    "type": "files",
    "files": [
        {
            "name": "20260203_223015_0_document.pdf",
            "url": "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf",
            "path": "/clipboard/file/20260203_223015_0_document.pdf"
        },
        {
            "name": "20260203_223015_1_image.jpg",
            "url": "http://192.168.31.3:35182/clipboard/file/20260203_223015_1_image.jpg",
            "path": "/clipboard/file/20260203_223015_1_image.jpg"
        }
    ]
}
```

**下载文件**:

```bash
# 下载单个文件
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf" \
  -o document.pdf
```

### 3. 文本剪贴板（保持兼容）

文本剪贴板功能保持不变，向后兼容。

**响应示例**（文本）:

```json
{
    "type": "text",
    "content": "Hello World",
    "length": 11,
    "empty": false
}
```

## 使用场景

### 场景 1：获取截图

```bash
# 1. 在 Windows 上按 Win+Shift+S 截图
# 2. 调用 API 获取剪贴板内容
curl -H "Authorization: Bearer $TOKEN" http://192.168.31.3:35182/clipboard

# 3. 返回图片 URL
# 4. 下载图片
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/clipboard/image/clipboard_images/clipboard_20260203_223015.png" \
  -o screenshot.png
```

### 场景 2：获取复制的文件

```bash
# 1. 在资源管理器中复制一个或多个文件
# 2. 调用 API 获取剪贴板内容
curl -H "Authorization: Bearer $TOKEN" http://192.168.31.3:35182/clipboard

# 3. 返回文件列表和 URL
# 4. 下载文件
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/clipboard/file/20260203_223015_0_document.pdf" \
  -o document.pdf
```

### 场景 3：获取文本

```bash
# 1. 复制文本到剪贴板
# 2. 调用 API 获取剪贴板内容
curl -H "Authorization: Bearer $TOKEN" http://192.168.31.3:35182/clipboard

# 3. 返回文本内容
```

## 技术细节

### 图片处理

- **支持格式**: CF_DIB, CF_BITMAP
- **保存格式**: PNG
- **保存位置**: `clipboard_images/` 目录
- **文件命名**: `clipboard_YYYYMMDD_HHMMSS.png`
- **使用 GDI+**: 高质量图像处理

### 文件处理

- **支持格式**: CF_HDROP（文件拖放格式）
- **保存位置**: `clipboard_files/` 目录
- **文件命名**: `YYYYMMDD_HHMMSS_序号_原文件名`
- **跳过目录**: 只复制文件，不复制目录
- **多文件支持**: 支持一次复制多个文件

### 自动检测

服务器会自动检测剪贴板内容类型：

1. 首先检查是否有图片（CF_DIB 或 CF_BITMAP）
2. 然后检查是否有文件（CF_HDROP）
3. 最后检查文本（CF_TEXT）

## API 端点

### 1. 获取剪贴板内容

```
GET /clipboard
Authorization: Bearer <token>
```

**响应类型**:

- `type: "image"` - 图片
- `type: "files"` - 文件列表
- `type: "text"` - 文本

### 2. 下载剪贴板图片

```
GET /clipboard/image/<filename>
Authorization: Bearer <token>
```

**响应**: PNG 图片文件（二进制）

### 3. 下载剪贴板文件

```
GET /clipboard/file/<filename>
Authorization: Bearer <token>
```

**响应**: 原始文件（二进制）

### 4. 设置剪贴板文本（保持不变）

```
PUT /clipboard
Authorization: Bearer <token>
Content-Type: application/json

{
  "content": "text to set"
}
```

## Python 示例

```python
import requests
import json

SERVER = "http://192.168.31.3:35182"
TOKEN = "your-token-here"
headers = {"Authorization": f"Bearer {TOKEN}"}

# 获取剪贴板内容
response = requests.get(f"{SERVER}/clipboard", headers=headers)
data = response.json()

if data["type"] == "image":
    # 下载图片
    image_url = data["url"]
    img_response = requests.get(image_url, headers=headers)
    with open("clipboard_image.png", "wb") as f:
        f.write(img_response.content)
    print(f"Image saved: clipboard_image.png")

elif data["type"] == "files":
    # 下载所有文件
    for file_info in data["files"]:
        file_url = file_info["url"]
        file_name = file_info["name"]
        file_response = requests.get(file_url, headers=headers)
        with open(file_name, "wb") as f:
            f.write(file_response.content)
        print(f"File saved: {file_name}")

elif data["type"] == "text":
    # 显示文本
    print(f"Text: {data['content']}")
```

## JavaScript 示例

```javascript
const SERVER = "http://192.168.31.3:35182";
const TOKEN = "your-token-here";
const headers = { Authorization: `Bearer ${TOKEN}` };

// 获取剪贴板内容
fetch(`${SERVER}/clipboard`, { headers })
    .then((res) => res.json())
    .then(async (data) => {
        if (data.type === "image") {
            // 下载图片
            const imgResponse = await fetch(data.url, { headers });
            const blob = await imgResponse.blob();
            const url = URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url;
            a.download = "clipboard_image.png";
            a.click();
        } else if (data.type === "files") {
            // 下载所有文件
            for (const file of data.files) {
                const fileResponse = await fetch(file.url, { headers });
                const blob = await fileResponse.blob();
                const url = URL.createObjectURL(blob);
                const a = document.createElement("a");
                a.href = url;
                a.download = file.name;
                a.click();
            }
        } else if (data.type === "text") {
            console.log("Text:", data.content);
        }
    });
```

## 注意事项

1. **认证**: 所有请求都需要 Bearer Token
2. **文件大小**: 没有明确的文件大小限制，但建议不要复制过大的文件
3. **存储空间**: 图片和文件会保存在服务器本地，注意磁盘空间
4. **清理**: 服务器不会自动清理旧文件，需要手动清理 `clipboard_images/` 和 `clipboard_files/` 目录
5. **并发**: 多个客户端同时访问时，文件名使用时间戳区分，不会冲突

## 故障排除

### Q: 获取剪贴板返回 "Failed to open clipboard"？

**A**: 剪贴板可能被其他程序占用，稍后重试。

### Q: 图片或文件 URL 返回 404？

**A**: 检查文件名是否正确，确保文件存在于 `clipboard_images/` 或 `clipboard_files/` 目录。

### Q: 复制的文件没有出现在响应中？

**A**: 确保复制的是文件而不是目录，服务器会跳过目录。

## 更新日志

### v0.2.0 (2026-02-03)

- ✅ 新增剪贴板图片支持
- ✅ 新增剪贴板文件支持
- ✅ 新增 `/clipboard/image/<filename>` 端点
- ✅ 新增 `/clipboard/file/<filename>` 端点
- ✅ 自动检测剪贴板内容类型
- ✅ 保持文本剪贴板向后兼容

## 相关文档

- [README.md](README.md) - 项目主文档
- [docs/API.md](docs/API.md) - 完整 API 文档
- [docs/Authentication.md](docs/Authentication.md) - 认证指南
- [QUICKSTART_AUTH.md](QUICKSTART_AUTH.md) - 快速开始
