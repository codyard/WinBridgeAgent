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


# WinBridgeAgent 部署脚本
# 用于将编译好的文件复制到其他电脑

echo "========================================"
echo "WinBridgeAgent Deployment Script"
echo "========================================"
echo ""

# 定义部署目标配置
# 格式: "电脑名称|挂载点|IP地址|端口"
DEPLOY_TARGETS=(
    "Test|/Volumes/Test|192.168.31.3|35182"
    # 添加更多电脑配置，例如:
    # "Office|/Volumes/Office|192.168.31.4|35182"
    # "Home|/Volumes/Home|192.168.31.5|35182"
    # "Laptop|/Volumes/Laptop|192.168.31.6|35182"
)

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 函数: 部署到指定目标
deploy_to_target() {
    local PC_NAME=$1
    local MOUNT_POINT=$2
    local IP_ADDRESS=$3
    local PORT=$4
    
    echo ""
    echo "========================================"
    echo "部署目标: $PC_NAME ($IP_ADDRESS:$PORT)"
    echo "========================================"
    
    # 检查挂载点是否存在
    if [ ! -d "$MOUNT_POINT" ]; then
        echo -e "${YELLOW}⚠ 挂载点 $MOUNT_POINT 不存在，跳过 $PC_NAME${NC}"
        return
    fi
    
    # 检测远程服务器上的程序是否在运行
    echo "检查远程服务器状态..."
    
    # 尝试访问健康检查端点
    if curl -s --connect-timeout 2 http://$IP_ADDRESS:$PORT/health > /dev/null 2>&1; then
        echo -e "${GREEN}✓ 服务器正在运行，准备发送退出命令...${NC}"
        
        # 尝试从已部署的 config.json 读取 token
        AUTH_TOKEN=""
        if [ -f "$MOUNT_POINT/WinBridgeAgent/config.json" ]; then
            echo "  读取认证令牌..."
            AUTH_TOKEN=$(python3 - <<PY
import json
try:
    with open("$MOUNT_POINT/WinBridgeAgent/config.json","r",encoding="utf-8") as f:
        data=json.load(f)
    token=data.get("auth_token") or data.get("security",{}).get("bearer_token") or ""
    print(token)
except Exception:
    print("")
PY
)
        fi

        # 发送退出指令（带 Authorization）
        if [ -n "$AUTH_TOKEN" ]; then
            echo "  发送认证退出命令..."
            curl -s -H "Authorization: Bearer $AUTH_TOKEN" http://$IP_ADDRESS:$PORT/exit > /dev/null 2>&1
        else
            echo "  发送退出命令（无认证）..."
            curl -s http://$IP_ADDRESS:$PORT/exit > /dev/null 2>&1
        fi
        
        # 等待服务器关闭
        echo "  等待服务器关闭..."
        sleep 2
        
        # 验证服务器已关闭
        for i in 1 2 3 4 5; do
            if curl -s --connect-timeout 2 http://$IP_ADDRESS:$PORT/health > /dev/null 2>&1; then
                echo "  服务器仍在运行... (${i}/5)"
                sleep 2
            else
                echo -e "${GREEN}✓ 服务器已成功关闭${NC}"
                break
            fi
        done
        
        # 主程序退出时会自动通知 Daemon 退出
        echo "  等待 Daemon 退出..."
        sleep 2
    else
        echo -e "${GREEN}✓ 服务器未运行${NC}"
    fi
    
    # 创建目标目录
    mkdir -p $MOUNT_POINT/WinBridgeAgent
    
    # 复制主程序
    echo ""
    echo "复制文件..."
    
    if [ -f "build/x64/WinBridgeAgent.exe" ]; then
        cp build/x64/WinBridgeAgent.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgent-x64.exe
        echo -e "${GREEN}✓ 已复制 x64 主程序${NC}"
    else
        echo -e "${RED}✗ 未找到 x64 主程序${NC}"
    fi

    if [ -f "build/x64/updater/Updater.exe" ]; then
        cp build/x64/updater/Updater.exe $MOUNT_POINT/WinBridgeAgent/Updater-x64.exe
        echo -e "${GREEN}✓ 已复制 x64 升级器${NC}"
    else
        echo -e "${YELLOW}⚠ 未找到 x64 升级器${NC}"
    fi

    # 检查 Daemon 进程是否在运行（通过文件锁定检测）
    if [ -f "build/x64/WinBridgeAgentDaemon.exe" ]; then
        DAEMON_TARGET="$MOUNT_POINT/WinBridgeAgent/WinBridgeAgentDaemon-x64.exe"
        if [ -f "$DAEMON_TARGET" ]; then
            # 尝试复制，如果文件被锁定会失败
            if cp build/x64/WinBridgeAgentDaemon.exe "$DAEMON_TARGET" 2>/dev/null; then
                echo -e "${GREEN}✓ 已复制 x64 守护进程${NC}"
            else
                echo -e "${YELLOW}⚠ x64 守护进程正在运行，跳过复制${NC}"
            fi
        else
            # 文件不存在，直接复制
            cp build/x64/WinBridgeAgentDaemon.exe "$DAEMON_TARGET"
            echo -e "${GREEN}✓ 已复制 x64 守护进程${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ 未找到 x64 守护进程${NC}"
    fi
    
    if [ -f "build/x86/WinBridgeAgent.exe" ]; then
        cp build/x86/WinBridgeAgent.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgent-x86.exe
        echo -e "${GREEN}✓ 已复制 x86 主程序${NC}"
    else
        echo -e "${YELLOW}⚠ 未找到 x86 主程序${NC}"
    fi

    if [ -f "build/x86/updater/Updater.exe" ]; then
        cp build/x86/updater/Updater.exe $MOUNT_POINT/WinBridgeAgent/Updater-x86.exe
        echo -e "${GREEN}✓ 已复制 x86 升级器${NC}"
    else
        echo -e "${YELLOW}⚠ 未找到 x86 升级器${NC}"
    fi

    # 检查 x86 Daemon 进程是否在运行
    if [ -f "build/x86/WinBridgeAgentDaemon.exe" ]; then
        DAEMON_TARGET="$MOUNT_POINT/WinBridgeAgent/WinBridgeAgentDaemon-x86.exe"
        if [ -f "$DAEMON_TARGET" ]; then
            # 尝试复制，如果文件被锁定会失败
            if cp build/x86/WinBridgeAgentDaemon.exe "$DAEMON_TARGET" 2>/dev/null; then
                echo -e "${GREEN}✓ 已复制 x86 守护进程${NC}"
            else
                echo -e "${YELLOW}⚠ x86 守护进程正在运行，跳过复制${NC}"
            fi
        else
            # 文件不存在，直接复制
            cp build/x86/WinBridgeAgentDaemon.exe "$DAEMON_TARGET"
            echo -e "${GREEN}✓ 已复制 x86 守护进程${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ 未找到 x86 守护进程${NC}"
    fi
    
    if [ -f "build-arm64/WinBridgeAgent.exe" ]; then
        cp build-arm64/WinBridgeAgent.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgent-arm64.exe
        echo -e "${GREEN}✓ 已复制 ARM64 主程序${NC}"
    fi

    if [ -f "build-arm64/updater/Updater.exe" ]; then
        cp build-arm64/updater/Updater.exe $MOUNT_POINT/WinBridgeAgent/Updater-arm64.exe
        echo -e "${GREEN}✓ 已复制 ARM64 升级器${NC}"
    fi

    # 检查 ARM64 Daemon 进程是否在运行
    if [ -f "build-arm64/WinBridgeAgentDaemon.exe" ]; then
        DAEMON_TARGET="$MOUNT_POINT/WinBridgeAgent/WinBridgeAgentDaemon-arm64.exe"
        if [ -f "$DAEMON_TARGET" ]; then
            # 尝试复制，如果文件被锁定会失败
            if cp build-arm64/WinBridgeAgentDaemon.exe "$DAEMON_TARGET" 2>/dev/null; then
                echo -e "${GREEN}✓ 已复制 ARM64 守护进程${NC}"
            else
                echo -e "${YELLOW}⚠ ARM64 守护进程正在运行，跳过复制${NC}"
            fi
        else
            # 文件不存在，直接复制
            cp build-arm64/WinBridgeAgentDaemon.exe "$DAEMON_TARGET"
            echo -e "${GREEN}✓ 已复制 ARM64 守护进程${NC}"
        fi
    fi
    
    # 复制配置模板（不覆盖已有配置）
    if [ -f "resources/config.template.json" ]; then
        cp resources/config.template.json $MOUNT_POINT/WinBridgeAgent/
        echo -e "${GREEN}✓ 已复制配置模板${NC}"
    fi

    # 复制翻译文件
    if [ -f "resources/translations.json" ]; then
        mkdir -p $MOUNT_POINT/WinBridgeAgent/resources
        cp resources/translations.json $MOUNT_POINT/WinBridgeAgent/resources/
        echo -e "${GREEN}✓ 已复制翻译文件${NC}"
    fi
    
    # 复制文档
    if [ -f "README.md" ]; then
        cp README.md $MOUNT_POINT/WinBridgeAgent/
        echo -e "${GREEN}✓ 已复制 README.md${NC}"
    fi
    
    if [ -f "FIREWALL.md" ]; then
        cp FIREWALL.md $MOUNT_POINT/WinBridgeAgent/
        echo -e "${GREEN}✓ 已复制 FIREWALL.md${NC}"
    fi
    
    # 显示部署结果
    echo ""
    echo -e "${GREEN}✓ 文件已部署到: $MOUNT_POINT/WinBridgeAgent/ ($PC_NAME)${NC}"
    echo ""
    echo "部署文件列表:"
    ls -lh $MOUNT_POINT/WinBridgeAgent/ | grep -E '\.(exe|json|md)$'
}

# 检查是否有编译文件
if [ ! -f "build/x64/WinBridgeAgent.exe" ] && [ ! -f "build/x86/WinBridgeAgent.exe" ]; then
    echo -e "${RED}错误: 未找到编译文件！${NC}"
    echo "请先运行 ./scripts/build.sh 进行编译"
    exit 1
fi

# 遍历所有部署目标
for target in "${DEPLOY_TARGETS[@]}"; do
    IFS='|' read -r PC_NAME MOUNT_POINT IP_ADDRESS PORT <<< "$target"
    deploy_to_target "$PC_NAME" "$MOUNT_POINT" "$IP_ADDRESS" "$PORT"
done

echo ""
echo "========================================"
echo -e "${GREEN}部署完成！${NC}"
echo "========================================"
echo ""

# 显示使用说明
echo "使用说明:"
echo "1. 编辑此脚本，在 DEPLOY_TARGETS 数组中添加更多部署目标"
echo "2. 格式: \"电脑名称|挂载点|IP地址|端口\""
echo "3. 确保目标电脑的共享文件夹已挂载到 /Volumes/"
echo ""
