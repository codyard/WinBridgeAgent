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


# ClawDesk MCP Server - 初始化脚本
# 用途：初始化私有仓库和公开仓库

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================="
echo "ClawDesk MCP Server 初始化脚本"
echo -e "==========================================${NC}"
echo ""

# 加载配置
if [ -f "release.config" ]; then
    source release.config
else
    echo -e "${RED}错误: release.config 不存在${NC}"
    exit 1
fi

# 步骤 1: 初始化私有仓库（当前目录）
echo -e "${BLUE}[1/3] 初始化私有仓库...${NC}"
echo ""

if [ -d ".git" ]; then
    echo -e "${YELLOW}⚠ 当前目录已经是 git 仓库${NC}"
    git status
else
    echo "即将初始化当前目录为 git 仓库（私有源代码仓库）"
    read -p "继续？(y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${RED}已取消${NC}"
        exit 1
    fi

    git init
    echo -e "${GREEN}✓ 已初始化 git 仓库${NC}"

    # 添加所有文件
    git add .
    git commit -m "Initial commit: ClawDesk MCP Server source code"
    echo -e "${GREEN}✓ 已创建初始提交${NC}"

    # 询问是否配置远程仓库
    echo ""
    echo -e "${YELLOW}是否配置私有仓库的远程地址？${NC}"
    read -p "继续？(y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "请输入私有仓库地址（例如：git@github.com:codyard/WinBridgeAgent-Private.git）："
        read PRIVATE_REPO_URL

        git remote add origin "$PRIVATE_REPO_URL"
        git branch -M main

        echo -e "${YELLOW}是否立即推送到远程？${NC}"
        read -p "继续？(y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git push -u origin main
            echo -e "${GREEN}✓ 已推送到远程仓库${NC}"
        fi
    fi
fi

echo ""

# 步骤 2: 构建程序
echo -e "${BLUE}[2/3] 构建程序...${NC}"
echo ""
echo "需要先构建程序才能生成发布文件"
read -p "是否立即构建？(y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    ./build.sh
    echo -e "${GREEN}✓ 构建完成${NC}"
else
    echo -e "${YELLOW}⚠ 跳过构建，请稍后手动运行 ./build.sh${NC}"
fi

echo ""

# 步骤 3: 生成发布文件并初始化公开仓库
echo -e "${BLUE}[3/3] 初始化公开仓库...${NC}"
echo ""

if [ ! -f "build/x64/WinBridgeAgent.exe" ]; then
    echo -e "${YELLOW}⚠ 构建产物不存在，跳过发布文件生成${NC}"
    echo "请先运行 ./build.sh 构建程序"
else
    echo "即将生成发布文件到 ${RELEASE_DIR}"
    read -p "继续？(y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        ./release.sh
        echo -e "${GREEN}✓ 发布文件生成完成${NC}"

        # 初始化公开仓库
        cd "${RELEASE_DIR}"

        if [ -d ".git" ]; then
            echo -e "${YELLOW}⚠ 公开仓库已经初始化${NC}"
        else
            git init
            git add .
            git commit -m "Initial release: ClawDesk MCP Server v${VERSION}"
            git tag "v${VERSION}"
            git branch -M main
            echo -e "${GREEN}✓ 已初始化公开仓库${NC}"

            # 配置远程仓库
            echo ""
            echo -e "${YELLOW}配置公开仓库远程地址...${NC}"
            echo "公开仓库地址: ${PUBLIC_REPO}"

            git remote add origin "${PUBLIC_REPO}"
            echo -e "${GREEN}✓ 已配置远程仓库${NC}"

            # 询问是否推送
            echo ""
            echo -e "${YELLOW}是否立即推送到公开仓库？${NC}"
            read -p "继续？(y/N) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                git push -u origin main --tags
                echo -e "${GREEN}✓ 已推送到公开仓库${NC}"
            else
                echo -e "${YELLOW}稍后可以手动推送：${NC}"
                echo "  cd ${RELEASE_DIR}"
                echo "  git push -u origin main --tags"
            fi
        fi

        cd - > /dev/null
    fi
fi

echo ""
echo -e "${GREEN}=========================================="
echo "✓ 初始化完成！"
echo -e "==========================================${NC}"
echo ""
echo -e "${YELLOW}下一步操作：${NC}"
echo ""
echo "1. 如果还没有构建程序，运行："
echo "   ./build.sh"
echo ""
echo "2. 如果还没有生成发布文件，运行："
echo "   ./release.sh"
echo ""
echo "3. 后续发布新版本，只需运行："
echo "   ./auto-release.sh v0.3.0"
echo ""
echo "4. 查看公开仓库："
echo "   ${PUBLIC_REPO}"
echo ""
echo -e "${GREEN}=========================================${NC}"
