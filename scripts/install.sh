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


# WinBridgeAgent Server - 自动安装和配置脚本
# 用途：自动下载、安装和配置 WinBridgeAgent Server

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================="
echo "WinBridgeAgent Server 自动安装脚本"
echo -e "==========================================${NC}"
echo ""

# 检测操作系统
OS="$(uname -s)"
case "${OS}" in
    Linux*)     PLATFORM=linux;;
    Darwin*)    PLATFORM=mac;;
    CYGWIN*)    PLATFORM=windows;;
    MINGW*)     PLATFORM=windows;;
    *)          PLATFORM="unknown"
esac

echo "检测到操作系统: ${PLATFORM}"
echo ""

# 配置
GITHUB_REPO="codyard/WinBridgeAgent"
INSTALL_DIR="$HOME/.winbridgeagent"
CONFIG_FILE=""

# 根据平台设置配置文件路径
if [ "${PLATFORM}" = "mac" ]; then
    CONFIG_FILE="$HOME/Library/Application Support/Claude/claude_desktop_config.json"
elif [ "${PLATFORM}" = "linux" ]; then
    CONFIG_FILE="$HOME/.config/Claude/claude_desktop_config.json"
elif [ "${PLATFORM}" = "windows" ]; then
    CONFIG_FILE="$APPDATA/Claude/claude_desktop_config.json"
fi

echo -e "${YELLOW}注意：${NC}"
echo "此脚本用于配置 MCP Client 连接到 WinBridgeAgent Server"
echo "WinBridgeAgent Server 需要在 Windows 上运行"
echo ""

if [ "${PLATFORM}" != "windows" ]; then
    echo -e "${YELLOW}你当前在 ${PLATFORM} 上运行此脚本${NC}"
    echo "请确保："
    echo "1. WinBridgeAgent Server 已在 Windows 机器上运行"
    echo "2. 你知道 Windows 机器的 IP 地址"
    echo "3. 你有 auth_token（从 Windows 机器的 config.json 中获取）"
    echo ""

    read -p "是否继续配置 MCP Client？(y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${RED}已取消${NC}"
        exit 1
    fi

    echo ""
    echo "请输入 Windows 机器的 IP 地址（例如：192.168.1.100）："
    read SERVER_IP

    echo "请输入 auth_token（从 Windows 机器的 config.json 中获取）："
    read AUTH_TOKEN

    SERVER_URL="http://${SERVER_IP}:35182"
else
    # Windows 平台
    echo -e "${YELLOW}Windows 平台检测${NC}"
    echo "请先手动下载并运行 WinBridgeAgent Server："
    echo "https://github.com/${GITHUB_REPO}/releases"
    echo ""

    SERVER_URL="http://localhost:35182"

    echo "请输入 auth_token（从 config.json 中获取）："
    read AUTH_TOKEN
fi

echo ""
echo -e "${BLUE}[1/3] 验证服务器连接...${NC}"

# 测试连接
if curl -s --connect-timeout 5 "${SERVER_URL}/health" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ 服务器连接成功${NC}"
else
    echo -e "${RED}✗ 无法连接到服务器: ${SERVER_URL}${NC}"
    echo "请确保："
    echo "1. WinBridgeAgent Server 正在运行"
    echo "2. 防火墙允许端口 35182"
    echo "3. IP 地址正确"
    exit 1
fi

echo ""
echo -e "${BLUE}[2/3] 配置 MCP Client...${NC}"

# 检查配置文件是否存在
if [ ! -f "${CONFIG_FILE}" ]; then
    echo -e "${YELLOW}⚠ Claude Desktop 配置文件不存在${NC}"
    echo "创建配置文件: ${CONFIG_FILE}"

    # 创建目录
    mkdir -p "$(dirname "${CONFIG_FILE}")"

    # 创建新配置
    cat > "${CONFIG_FILE}" << EOF
{
  "mcpServers": {
    "winbridgeagent": {
      "url": "${SERVER_URL}",
      "transport": "http",
      "headers": {
        "Authorization": "Bearer ${AUTH_TOKEN}"
      }
    }
  }
}
EOF
    echo -e "${GREEN}✓ 已创建配置文件${NC}"
else
    echo -e "${YELLOW}⚠ 配置文件已存在${NC}"
    echo "配置文件: ${CONFIG_FILE}"
    echo ""
    echo "请手动编辑配置文件，添加以下内容："
    echo ""
    echo -e "${BLUE}----------------------------------------${NC}"
    cat << EOF
{
  "mcpServers": {
    "winbridgeagent": {
      "url": "${SERVER_URL}",
      "transport": "http",
      "headers": {
        "Authorization": "Bearer ${AUTH_TOKEN}"
      }
    }
  }
}
EOF
    echo -e "${BLUE}----------------------------------------${NC}"
    echo ""
    echo "如果配置文件中已有其他 MCP Server，请将 winbridgeagent 部分添加到 mcpServers 中"
fi

echo ""
echo -e "${BLUE}[3/3] 验证配置...${NC}"

# 测试 API
if curl -s -H "Authorization: Bearer ${AUTH_TOKEN}" "${SERVER_URL}/status" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ 认证成功${NC}"
else
    echo -e "${RED}✗ 认证失败${NC}"
    echo "请检查 auth_token 是否正确"
    exit 1
fi

echo ""
echo -e "${GREEN}=========================================="
echo "✓ 安装和配置完成！"
echo -e "==========================================${NC}"
echo ""
echo -e "${YELLOW}下一步：${NC}"
echo ""
echo "1. 重启 Claude Desktop"
echo ""
echo "2. 在 Claude Desktop 中测试："
echo "   \"请帮我读取剪贴板内容\""
echo ""
echo "3. 查看配置文件："
echo "   ${CONFIG_FILE}"
echo ""
echo "4. 查看服务器状态："
echo "   ${SERVER_URL}/status"
echo ""
echo -e "${GREEN}=========================================${NC}"
