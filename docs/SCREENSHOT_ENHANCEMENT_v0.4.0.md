# 截图功能增强 - v0.4.0

## 概述

v0.4.0 版本增强了截图功能，新增了 `/help` 端点和两个强大的 MCP 截图工具。

## 新增功能

### 1. `/help` 端点（HTTP API）

**端点**: `GET /` 或 `GET /help`

返回完整的 API 端点列表和 MCP 工具列表，方便开发者快速了解所有可用功能。

**响应示例**:

```json
{
  "name": "ClawDesk MCP Server",
  "version": "0.4.0",
  "status": "running",
  "endpoints": [
    {"path": "/", "method": "GET", "description": "API endpoint list"},
    {"path": "/help", "method": "GET", "description": "API endpoint list"},
    ...
  ],
  "mcp_tools": [
    {"name": "take_screenshot", "description": "Take a full screen screenshot"},
    {"name": "take_screenshot_window", "description": "Take screenshot of a window by title"},
    {"name": "take_screenshot_region", "description": "Take screenshot of a region"},
    ...
  ]
}
```

---

### 2. 指定窗口截图（MCP 工具）

**工具名称**: `take_screenshot_window`

**功能**: 通过窗口标题捕获指定窗口的截图

**参数**:

```json
{
    "title": "窗口标题"
}
```

**特性**:

- ✅ 支持精确匹配和模糊匹配（不区分大小写）
- ✅ 自动恢复最小化的窗口
- ✅ 自动将窗口置于前台
- ✅ 使用 PrintWindow API 捕获完整内容

**使用示例**:

```json
// 捕获 Chrome 浏览器窗口
{
  "name": "take_screenshot_window",
  "arguments": {
    "title": "Chrome"
  }
}

// 捕获记事本窗口
{
  "name": "take_screenshot_window",
  "arguments": {
    "title": "notepad"
  }
}
```

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 1024,
    "height": 768,
    "created_at": "2026-02-05T02:25:30Z"
}
```

---

### 3. 指定区域截图（MCP 工具）

**工具名称**: `take_screenshot_region`

**功能**: 捕获屏幕上指定矩形区域的截图

**参数**:

```json
{
    "x": 100,
    "y": 100,
    "width": 800,
    "height": 600
}
```

**坐标系统**:

- 原点 (0, 0) 在屏幕左上角
- X 轴向右增加，Y 轴向下增加

**使用示例**:

```json
// 捕获屏幕左上角 800x600 区域
{
  "name": "take_screenshot_region",
  "arguments": {
    "x": 0,
    "y": 0,
    "width": 800,
    "height": 600
  }
}

// 捕获屏幕中心区域（假设屏幕为 1920x1080）
{
  "name": "take_screenshot_region",
  "arguments": {
    "x": 448,
    "y": 156,
    "width": 1024,
    "height": 768
  }
}
```

**响应**:

```json
{
    "path": "screenshots/screenshot_20260205_102530.png",
    "width": 800,
    "height": 600,
    "created_at": "2026-02-05T02:25:30Z"
}
```

---

## 截图功能对比

| 功能         | HTTP API (/screenshot) | MCP: take_screenshot_window | MCP: take_screenshot_region |
| ------------ | ---------------------- | --------------------------- | --------------------------- |
| 全屏截图     | ✅                     | ❌                          | ❌                          |
| 窗口截图     | ❌                     | ✅                          | ❌                          |
| 区域截图     | ❌                     | ❌                          | ✅                          |
| 格式选择     | ✅ (png/jpg)           | ✅ (png)                    | ✅ (png)                    |
| 返回 URL     | ✅                     | ❌                          | ❌                          |
| 返回文件路径 | ✅                     | ✅                          | ✅                          |
| 窗口匹配     | ❌                     | ✅ (精确+模糊)              | ❌                          |
| 坐标控制     | ❌                     | ❌                          | ✅                          |

---

## 使用场景

### 1. 全屏截图

**适用场景**: 快速捕获整个屏幕，适合监控和记录

**调用方式**: HTTP API

```bash
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/screenshot"
```

---

### 2. 窗口截图

**适用场景**: 捕获特定应用程序窗口，适合文档和演示

**调用方式**: MCP 工具

```json
POST /mcp/tools/call
{
  "name": "take_screenshot_window",
  "arguments": {
    "title": "Chrome"
  }
}
```

**优势**:

- 无需手动切换窗口
- 自动处理最小化窗口
- 支持模糊匹配，无需精确标题

---

### 3. 区域截图

**适用场景**: 精确捕获屏幕特定区域，适合裁剪和局部截图

**调用方式**: MCP 工具

```json
POST /mcp/tools/call
{
  "name": "take_screenshot_region",
  "arguments": {
    "x": 100,
    "y": 100,
    "width": 800,
    "height": 600
  }
}
```

**优势**:

- 精确控制截图范围
- 减少文件大小
- 适合批量处理

---

## 技术实现

### 代码结构

- **头文件**: `include/services/screenshot_service.h`
- **实现文件**: `src/services/screenshot_service.cpp`
- **MCP 注册**: `src/main.cpp` (工具注册部分)

### 核心方法

```cpp
class ScreenshotService {
public:
    // 全屏截图
    ScreenshotResult captureFullScreen();

    // 窗口截图（通过标题）
    ScreenshotResult captureWindowByTitle(const std::string& title);

    // 区域截图
    ScreenshotResult captureRegion(int x, int y, int width, int height);
};
```

### 窗口匹配算法

1. 枚举所有可见窗口
2. 获取窗口标题并转换为小写
3. 优先精确匹配
4. 如果没有精确匹配，使用部分匹配
5. 返回第一个匹配的窗口

---

## 文档更新

已更新以下文档：

- ✅ `docs/API.md` - 添加 `/help` 端点和截图功能详细说明
- ✅ `docs/MCP.md` - 添加 `take_screenshot_window` 和 `take_screenshot_region` 工具说明
- ✅ `README.md` - 更新截图功能概述和使用示例
- ✅ `CHANGELOG.md` - 记录 v0.4.0 新增功能

---

## 构建和部署

### 构建命令

```bash
./scripts/build.sh
```

### 构建结果

- x64 版本: `build/x64/ClawDeskMCP.exe` (2.1MB)
- x86 版本: `build/x86/ClawDeskMCP.exe` (2.3MB)

### 部署位置

- `/Volumes/Test/ClawDeskMCP/` (Test 电脑 - 192.168.31.3)

---

## 测试建议

### 1. 测试 `/help` 端点

```bash
curl -H "Authorization: Bearer $TOKEN" \
  "http://192.168.31.3:35182/help"
```

### 2. 测试窗口截图

```bash
# 打开记事本
notepad.exe

# 调用 MCP 工具
curl -X POST \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"take_screenshot_window","arguments":{"title":"notepad"}}' \
  "http://192.168.31.3:35182/mcp/tools/call"
```

### 3. 测试区域截图

```bash
# 捕获屏幕左上角 500x500 区域
curl -X POST \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"take_screenshot_region","arguments":{"x":0,"y":0,"width":500,"height":500}}' \
  "http://192.168.31.3:35182/mcp/tools/call"
```

---

## 已知限制

1. **窗口截图**: 某些全屏应用（如游戏）可能无法正确捕获
2. **区域截图**: 坐标超出屏幕范围会导致错误
3. **格式支持**: MCP 工具仅支持 PNG 格式

---

## 未来改进

- [ ] 支持多显示器截图
- [ ] 支持 JPEG 格式（MCP 工具）
- [ ] 支持截图质量参数
- [ ] 支持截图后自动上传到云存储
- [ ] 支持截图历史记录管理

---

## 版本信息

- **版本**: v0.4.0
- **发布日期**: 2026-02-05
- **构建时间**: 10:55
- **工具总数**: 24 个（新增 2 个截图工具）

---

## 相关文档

- [API 文档](docs/API.md)
- [MCP 协议文档](docs/MCP.md)
- [README](README.md)
- [更新日志](CHANGELOG.md)
