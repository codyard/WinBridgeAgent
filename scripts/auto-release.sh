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


# ClawDesk MCP Server - 一键发布脚本
# 用途：自动化整个发布流程

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 加载配置
if [ -f "release.config" ]; then
    source release.config
else
    # 默认配置
    RELEASE_DIR="../WinBridgeAgent-Release"
    PUBLIC_REPO="https://github.com/codyard/WinBridgeAgent.git"
fi

# 检查参数
if [ -z "$1" ]; then
    echo -e "${RED}错误: 请指定版本号${NC}"
    echo "用法: ./auto-release.sh v0.3.0"
    exit 1
fi

VERSION=$1
VERSION_NUMBER=${VERSION#v}  # 去掉 v 前缀

echo -e "${BLUE}=========================================="
echo "ClawDesk MCP Server 一键发布脚本"
echo "版本: ${VERSION}"
echo -e "==========================================${NC}"
echo ""

# 确认发布
echo -e "${YELLOW}即将发布版本 ${VERSION}，是否继续？${NC}"
echo "这将执行以下操作："
echo "  1. 更新版本号"
echo "  2. 构建所有架构"
echo "  3. 生成发布文件"
echo "  4. 提交到私有仓库"
echo "  5. 提交到公开仓库"
echo ""
read -p "继续？(y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${RED}已取消${NC}"
    exit 1
fi

echo ""

# 步骤 1: 更新版本号
echo -e "${BLUE}[1/6] 更新版本号...${NC}"

# 更新 release.sh
if [ -f "release.sh" ]; then
    sed -i '' "s/VERSION=\".*\"/VERSION=\"${VERSION_NUMBER}\"/" release.sh
    echo -e "${GREEN}✓ 已更新 release.sh${NC}"
else
    echo -e "${RED}✗ release.sh 不存在${NC}"
    exit 1
fi

# 更新 CMakeLists.txt
if [ -f "CMakeLists.txt" ]; then
    sed -i '' "s/VERSION [0-9.]\+/VERSION ${VERSION_NUMBER}/" CMakeLists.txt
    echo -e "${GREEN}✓ 已更新 CMakeLists.txt${NC}"
else
    echo -e "${RED}✗ CMakeLists.txt 不存在${NC}"
    exit 1
fi

echo ""

# 步骤 2: 构建
echo -e "${BLUE}[2/6] 构建所有架构...${NC}"
if ./build.sh; then
    echo -e "${GREEN}✓ 构建成功${NC}"
else
    echo -e "${RED}✗ 构建失败${NC}"
    exit 1
fi

echo ""

# 步骤 3: 生成发布文件
echo -e "${BLUE}[3/6] 生成发布文件...${NC}"
if ./release.sh; then
    echo -e "${GREEN}✓ 发布文件生成成功${NC}"
else
    echo -e "${RED}✗ 发布文件生成失败${NC}"
    exit 1
fi

echo ""

# 步骤 4: 提交到私有仓库
echo -e "${BLUE}[4/6] 提交到私有仓库...${NC}"

# 检查是否有未提交的更改
if [ -n "$(git status --porcelain)" ]; then
    git add .
    git commit -m "Release ${VERSION}"
    echo -e "${GREEN}✓ 已提交更改${NC}"
else
    echo -e "${YELLOW}⚠ 没有需要提交的更改${NC}"
fi

# 创建标签
if git tag -l | grep -q "^${VERSION}$"; then
    echo -e "${YELLOW}⚠ 标签 ${VERSION} 已存在，跳过创建${NC}"
else
    git tag ${VERSION}
    echo -e "${GREEN}✓ 已创建标签 ${VERSION}${NC}"
fi

# 推送到远程
if git remote | grep -q "origin"; then
    echo -e "${YELLOW}推送到远程仓库...${NC}"
    git push origin main --tags
    echo -e "${GREEN}✓ 已推送到私有仓库${NC}"
else
    echo -e "${YELLOW}⚠ 未配置远程仓库，正在配置...${NC}"
    if [ -n "${PRIVATE_REPO}" ]; then
        git remote add origin "${PRIVATE_REPO}"
        echo -e "${GREEN}✓ 已配置远程仓库: ${PRIVATE_REPO}${NC}"
        git push -u origin main --tags
        echo -e "${GREEN}✓ 已推送到私有仓库${NC}"
    else
        echo -e "${RED}✗ 未配置私有仓库地址${NC}"
        echo "请在 release.config 中配置 PRIVATE_REPO"
        echo "或手动运行："
        echo "  git remote add origin <私有仓库地址>"
        echo "  git push -u origin main --tags"
    fi
fi

echo ""

# 步骤 5: 提交到公开仓库
echo -e "${BLUE}[5/6] 提交到公开仓库...${NC}"

if [ ! -d "${RELEASE_DIR}" ]; then
    echo -e "${RED}✗ 发布目录不存在: ${RELEASE_DIR}${NC}"
    exit 1
fi

cd "${RELEASE_DIR}"

# 检查是否是 git 仓库
if [ ! -d ".git" ]; then
    echo -e "${YELLOW}⚠ 公开仓库未初始化，正在初始化...${NC}"
    git init
    git branch -M main
    echo -e "${GREEN}✓ 已初始化 git 仓库${NC}"
fi

# 提交更改
if [ -n "$(git status --porcelain)" ]; then
    git add .
    git commit -m "Release ${VERSION}"
    echo -e "${GREEN}✓ 已提交更改${NC}"
else
    echo -e "${YELLOW}⚠ 没有需要提交的更改${NC}"
fi

# 创建标签
if git tag -l | grep -q "^${VERSION}$"; then
    echo -e "${YELLOW}⚠ 标签 ${VERSION} 已存在，跳过创建${NC}"
else
    git tag ${VERSION}
    echo -e "${GREEN}✓ 已创建标签 ${VERSION}${NC}"
fi

# 推送到远程
if git remote | grep -q "origin"; then
    echo -e "${YELLOW}推送到远程仓库...${NC}"
    git push origin main --tags
    echo -e "${GREEN}✓ 已推送到公开仓库${NC}"
else
    echo -e "${YELLOW}⚠ 未配置远程仓库，正在配置...${NC}"
    if [ -n "${PUBLIC_REPO}" ]; then
        git remote add origin "${PUBLIC_REPO}"
        echo -e "${GREEN}✓ 已配置远程仓库: ${PUBLIC_REPO}${NC}"
        git push -u origin main --tags
        echo -e "${GREEN}✓ 已推送到公开仓库${NC}"
    else
        echo -e "${RED}✗ 未配置公开仓库地址${NC}"
        echo "请在 release.config 中配置 PUBLIC_REPO"
        echo "或手动运行："
        echo "  cd ${RELEASE_DIR}"
        echo "  git remote add origin <公开仓库地址>"
        echo "  git push -u origin main --tags"
    fi
fi

cd - > /dev/null

echo ""

# 步骤 6: 完成
echo -e "${BLUE}[6/6] 发布完成！${NC}"
echo ""
echo -e "${GREEN}=========================================="
echo "✓ 发布成功！"
echo -e "==========================================${NC}"
echo ""
echo -e "${YELLOW}下一步操作：${NC}"
echo ""
echo "1. 访问 GitHub 公开仓库创建 Release："
echo "   https://github.com/YourUsername/WinBridgeAgent-Release/releases/new"
echo ""
echo "2. 选择标签: ${VERSION}"
echo ""
echo "3. 上传以下文件："
echo "   - ${RELEASE_DIR}/bin/WinBridgeAgent-x64.exe"
echo "   - ${RELEASE_DIR}/bin/WinBridgeAgent-x86.exe"
if [ -f "${RELEASE_DIR}/bin/WinBridgeAgent-arm64.exe" ]; then
    echo "   - ${RELEASE_DIR}/bin/WinBridgeAgent-arm64.exe"
fi
echo ""
echo "4. 填写 Release 说明并发布"
echo ""
echo -e "${GREEN}=========================================${NC}"
