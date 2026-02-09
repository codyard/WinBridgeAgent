# 版本管理说明

## 自动版本递增

从现在开始，每次运行 `./scripts/build.sh` 构建脚本时，版本号会自动递增 **0.0.1**。

## 版本号格式

版本号采用语义化版本格式：`MAJOR.MINOR.PATCH`

- **MAJOR**: 主版本号（重大变更）
- **MINOR**: 次版本号（新功能）
- **PATCH**: 补丁版本号（bug 修复和小改进）

## 自动递增规则

每次构建时，**PATCH** 版本号自动 +1：

```
0.4.0 → 0.4.1 → 0.4.2 → 0.4.3 → ...
```

## 构建流程

### 1. 运行构建脚本

```bash
./scripts/build.sh
```

### 2. 版本递增过程

```
Incrementing version number...
Current version: 0.4.0
New version: 0.4.1
✓ Version updated to 0.4.1
```

### 3. 自动更新位置

版本号会自动更新到以下位置：

- `CMakeLists.txt` - 项目版本定义
- 编译后的可执行文件（通过 `CLAWDESK_VERSION` 宏）

### 4. 版本号使用位置

编译后，版本号会出现在：

- HTTP API 响应（`/status`, `/help`）
- MCP 协议响应（`/mcp/initialize`）
- 托盘图标提示
- 关于对话框
- Settings 窗口

## 手动调整版本号

如果需要手动调整版本号（例如发布新的主版本或次版本），直接编辑 `CMakeLists.txt`：

```cmake
project(ClawDeskMCP VERSION 0.5.0 LANGUAGES CXX)
```

下次构建时会从新版本号继续递增：

```
0.5.0 → 0.5.1 → 0.5.2 → ...
```

## 版本历史追踪

建议在每次重要版本发布时更新 `CHANGELOG.md`：

```markdown
## [0.5.0] - 2026-02-05

### 新增

- 新功能描述

### 修复

- Bug 修复描述
```

## 构建脚本实现

版本递增逻辑在 `scripts/build.sh` 中实现：

```bash
# 读取当前版本号
CURRENT_VERSION=$(grep "^project(ClawDeskMCP VERSION" CMakeLists.txt | ...)

# 分解版本号
MAJOR=$(echo $CURRENT_VERSION | cut -d. -f1)
MINOR=$(echo $CURRENT_VERSION | cut -d. -f2)
PATCH=$(echo $CURRENT_VERSION | cut -d. -f3)

# 递增 patch 版本号
NEW_PATCH=$((PATCH + 1))
NEW_VERSION="$MAJOR.$MINOR.$NEW_PATCH"

# 更新 CMakeLists.txt
sed -i.bak "s/project(ClawDeskMCP VERSION $CURRENT_VERSION/project(ClawDeskMCP VERSION $NEW_VERSION/" CMakeLists.txt
```

## 多电脑部署

构建脚本支持同时部署到多台电脑，版本号在所有目标电脑上保持一致。

配置部署目标（在 `scripts/build.sh` 中）：

```bash
DEPLOY_TARGETS=(
    "Test|/Volumes/Test|192.168.31.3|35182"
    "Office|/Volumes/Office|192.168.31.4|35182"
    "Home|/Volumes/Home|192.168.31.5|35182"
)
```

## 版本查询

### HTTP API

```bash
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.31.3:35182/status
```

响应：

```json
{
  "status": "running",
  "version": "0.4.1",
  ...
}
```

### MCP 协议

```bash
curl -X POST \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}' \
  http://192.168.31.3:35182/mcp/initialize
```

响应：

```json
{
  "protocolVersion": "2024-11-05",
  "serverInfo": {
    "name": "ClawDesk MCP Server",
    "version": "0.4.1"
  },
  ...
}
```

## 注意事项

1. **Git 提交**: 版本号变更会修改 `CMakeLists.txt`，记得提交到版本控制
2. **构建频率**: 每次构建都会递增版本号，建议只在需要部署时构建
3. **版本回退**: 如果需要回退版本，手动编辑 `CMakeLists.txt` 即可
4. **备份文件**: 构建脚本会创建 `CMakeLists.txt.bak` 备份文件，但会自动删除

## 示例

### 第一次构建

```bash
$ ./scripts/build.sh
Incrementing version number...
Current version: 0.4.0
New version: 0.4.1
✓ Version updated to 0.4.1
...
```

### 第二次构建

```bash
$ ./scripts/build.sh
Incrementing version number...
Current version: 0.4.1
New version: 0.4.2
✓ Version updated to 0.4.2
...
```

### 手动调整到新的次版本

编辑 `CMakeLists.txt`：

```cmake
project(ClawDeskMCP VERSION 0.5.0 LANGUAGES CXX)
```

### 继续构建

```bash
$ ./scripts/build.sh
Incrementing version number...
Current version: 0.5.0
New version: 0.5.1
✓ Version updated to 0.5.1
...
```

## 相关文件

- `CMakeLists.txt` - 版本号定义
- `scripts/build.sh` - 构建脚本（包含版本递增逻辑）
- `CHANGELOG.md` - 版本历史记录
- `src/main.cpp` - 版本号使用（通过 `CLAWDESK_VERSION` 宏）
- `src/support/settings_window.cpp` - 版本号显示

## 版本发布流程

1. **开发阶段**: 每次构建自动递增 patch 版本
2. **功能完成**: 手动调整到新的 minor 版本（如 0.5.0）
3. **重大更新**: 手动调整到新的 major 版本（如 1.0.0）
4. **更新文档**: 更新 `CHANGELOG.md` 记录变更
5. **Git 提交**: 提交版本号变更和文档更新
6. **部署发布**: 运行构建脚本部署到目标电脑
