#!/bin/bash
# Copyright (C) 2026 Codyard
#
# This file is part of WinBridgeAgent.
#
# WinBridgeAgent is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# WinBridgeAgent is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with WinBridgeAgent. If not, see <https://www.gnu.org/licenses/\>.


# ClawDesk MCP Server - 发布脚本
# 用途：将编译好的程序和文档复制到公开发布目录

set -e  # 遇到错误立即退出

# 配置
RELEASE_DIR="../WinBridgeAgent-Release"
VERSION="0.3.0"

echo "=========================================="
echo "ClawDesk MCP Server 发布脚本"
echo "版本: ${VERSION}"
echo "=========================================="
echo ""

# 1. 检查构建产物是否存在
echo "1. 检查构建产物..."
if [ ! -f "build/x64/WinBridgeAgent.exe" ]; then
    echo "❌ 错误: build/x64/WinBridgeAgent.exe 不存在"
    echo "   请先运行 ./build.sh 构建程序"
    exit 1
fi

if [ ! -f "build/x86/WinBridgeAgent.exe" ]; then
    echo "❌ 错误: build/x86/WinBridgeAgent.exe 不存在"
    echo "   请先运行 ./build.sh 构建程序"
    exit 1
fi

echo "✓ 构建产物检查通过"
echo ""

# 2. 创建发布目录结构
echo "2. 创建发布目录结构..."
mkdir -p "${RELEASE_DIR}"
mkdir -p "${RELEASE_DIR}/bin"
mkdir -p "${RELEASE_DIR}/docs"
mkdir -p "${RELEASE_DIR}/examples"

echo "✓ 发布目录创建完成"
echo ""

# 3. 复制可执行文件
echo "3. 复制可执行文件..."
cp build/x64/WinBridgeAgent.exe "${RELEASE_DIR}/bin/WinBridgeAgent-x64.exe"
cp build/x86/WinBridgeAgent.exe "${RELEASE_DIR}/bin/WinBridgeAgent-x86.exe"

# 如果有 ARM64 版本也复制
if [ -f "build-arm64/WinBridgeAgent.exe" ]; then
    cp build-arm64/WinBridgeAgent.exe "${RELEASE_DIR}/bin/WinBridgeAgent-arm64.exe"
    echo "✓ 已复制 x64, x86, ARM64 版本"
else
    echo "✓ 已复制 x64, x86 版本"
fi
echo ""

# 4. 复制用户文档
echo "4. 复制用户文档..."
cp README.md "${RELEASE_DIR}/"
cp LICENSE "${RELEASE_DIR}/"
cp FIREWALL.md "${RELEASE_DIR}/docs/"
cp DASHBOARD_GUIDE.md "${RELEASE_DIR}/docs/"
cp DASHBOARD_TEST.md "${RELEASE_DIR}/docs/"

# 复制 MCP Client 配置文件
cp configs/mcp-config-template.json "${RELEASE_DIR}/"
cp MCP_CLIENT_SETUP.md "${RELEASE_DIR}/"
cp SETUP_MCP_CLIENT.md "${RELEASE_DIR}/"
cp install.sh "${RELEASE_DIR}/"
cp DOCS_INDEX.md "${RELEASE_DIR}/"

# 复制 docs 目录下的文档
if [ -d "docs" ]; then
    cp docs/API.md "${RELEASE_DIR}/docs/" 2>/dev/null || true
    cp docs/Dashboard.md "${RELEASE_DIR}/docs/" 2>/dev/null || true
    cp docs/MCP.md "${RELEASE_DIR}/docs/" 2>/dev/null || true
fi

echo "✓ 用户文档复制完成"
echo ""

# 5. 复制配置模板
echo "5. 复制配置模板..."
if [ -f "resources/config.template.json" ]; then
    cp resources/config.template.json "${RELEASE_DIR}/config.template.json"
    echo "✓ 配置模板复制完成"
else
    echo "⚠ 警告: resources/config.template.json 不存在"
fi
echo ""

# 6. 复制 MCP 协议说明文档
echo "6. 复制 MCP 协议说明文档..."
if [ -d ".kiro/specs/clawdesk-mcp-server" ]; then
    cp .kiro/specs/clawdesk-mcp-server/requirements.md "${RELEASE_DIR}/docs/MCP-Requirements.md"
    echo "✓ MCP 需求文档复制完成"
fi
echo ""

# 7. 创建版本信息文件
echo "7. 创建版本信息文件..."
cat > "${RELEASE_DIR}/VERSION.txt" << EOF
ClawDesk MCP Server
版本: ${VERSION}
构建时间: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
构建平台: $(uname -s) $(uname -m)

包含文件:
- WinBridgeAgent-x64.exe (Windows x64)
- WinBridgeAgent-x86.exe (Windows x86)
EOF

if [ -f "build-arm64/WinBridgeAgent.exe" ]; then
    echo "- WinBridgeAgent-arm64.exe (Windows ARM64)" >> "${RELEASE_DIR}/VERSION.txt"
fi

echo "✓ 版本信息文件创建完成"
echo ""

# 8. 创建 README 文件（公开仓库专用）
echo "8. 创建公开仓库 README..."
cat > "${RELEASE_DIR}/README-PUBLIC.md" << 'EOF'
# ClawDesk MCP Server

ClawDesk MCP Server 是一个 Windows 本地能力服务，遵循 Model Context Protocol (MCP) 标准协议，为 AI 助理（如 OpenClaw、Claude Desktop）提供安全、可控、可追溯的 Windows 系统操作能力。

## 下载

请从 [Releases](../../releases) 页面下载最新版本。

### 版本选择

- **WinBridgeAgent-x64.exe**: 适用于 64 位 Windows 系统（推荐）
- **WinBridgeAgent-x86.exe**: 适用于 32 位 Windows 系统
- **WinBridgeAgent-arm64.exe**: 适用于 ARM64 Windows 系统（如 Surface Pro X）

## 快速开始

1. 下载对应版本的可执行文件
2. 双击运行，程序会在系统托盘显示图标
3. 首次运行会自动生成 `config.json` 配置文件
4. 右键点击托盘图标可以查看状态和配置

详细使用说明请参考 [README.md](README.md)。

## 文档

- [README.md](README.md) - 完整使用指南
- [docs/API.md](docs/API.md) - HTTP API 文档
- [docs/Dashboard.md](docs/Dashboard.md) - Dashboard 使用指南
- [docs/MCP-Requirements.md](docs/MCP-Requirements.md) - MCP 协议需求说明
- [FIREWALL.md](FIREWALL.md) - 防火墙配置指南

## 系统要求

- Windows 10/11 (x64, x86, ARM64)
- 无需额外运行时依赖

## 许可证

Copyright © 2026 ClawDesk

详见 [LICENSE](LICENSE) 文件。

## 支持

如有问题或建议，请提交 [Issue](../../issues)。
EOF

echo "✓ 公开仓库 README 创建完成"
echo ""

# 9. 创建 .gitignore（公开仓库专用）
echo "9. 创建 .gitignore..."
cat > "${RELEASE_DIR}/.gitignore" << 'EOF'
# 运行时生成的文件
config.json
runtime.json
usage.json
logs/
screenshots/

# macOS
.DS_Store

# Windows
Thumbs.db
desktop.ini
EOF

echo "✓ .gitignore 创建完成"
echo ""

# 10. 显示发布目录内容
echo "10. 发布目录内容:"
echo "=========================================="
tree -L 2 "${RELEASE_DIR}" 2>/dev/null || find "${RELEASE_DIR}" -type f | sort
echo "=========================================="
echo ""

# 11. 显示文件大小
echo "11. 可执行文件大小:"
ls -lh "${RELEASE_DIR}/bin/"*.exe
echo ""

# 12. 完成提示
echo "=========================================="
echo "✓ 发布准备完成！"
echo ""
echo "发布目录: ${RELEASE_DIR}"
echo ""
echo "下一步操作:"
echo "1. cd ${RELEASE_DIR}"
echo "2. git init"
echo "3. git add ."
echo "4. git commit -m \"Release v${VERSION}\""
echo "5. git remote add origin <公开仓库地址>"
echo "6. git push -u origin main"
echo ""
echo "或者如果已经是 git 仓库:"
echo "1. cd ${RELEASE_DIR}"
echo "2. git add ."
echo "3. git commit -m \"Release v${VERSION}\""
echo "4. git tag v${VERSION}"
echo "5. git push origin main --tags"
echo "=========================================="
