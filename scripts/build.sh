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


# 版本号自动递增
echo "Incrementing version number..."

# 读取当前版本号
CURRENT_VERSION=$(grep "^project(WinBridgeAgent VERSION" CMakeLists.txt | sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/')
echo "Current version: $CURRENT_VERSION"

# 分解版本号
MAJOR=$(echo $CURRENT_VERSION | cut -d. -f1)
MINOR=$(echo $CURRENT_VERSION | cut -d. -f2)
PATCH=$(echo $CURRENT_VERSION | cut -d. -f3)

# 递增 patch 版本号
NEW_PATCH=$((PATCH + 1))
NEW_VERSION="$MAJOR.$MINOR.$NEW_PATCH"

echo "New version: $NEW_VERSION"

# 更新 CMakeLists.txt
sed -i.bak "s/project(WinBridgeAgent VERSION $CURRENT_VERSION/project(WinBridgeAgent VERSION $NEW_VERSION/" CMakeLists.txt
rm -f CMakeLists.txt.bak

echo "✓ Version updated to $NEW_VERSION"
echo ""

# 清理旧构建（包括 build/x64、build/x86 避免复用旧 cmake 缓存）
rm -rf build-*
rm -rf build/x64 build/x86 build/arm64

# 构建 x64 版本
echo "Building x64 version..."
mkdir -p build/x64
cd build/x64
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x64.cmake \
         -DCMAKE_BUILD_TYPE=Release \
         -DCLAWDESK_ENABLE_OPENSSL=OFF
make -j$(sysctl -n hw.ncpu)
cd ../..

# 构建 x86 版本
echo "Building x86 version..."
mkdir -p build/x86
cd build/x86
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-x86.cmake \
         -DCMAKE_BUILD_TYPE=Release \
         -DCLAWDESK_ENABLE_OPENSSL=OFF
make -j$(sysctl -n hw.ncpu)
cd ../..

# 构建 ARM64 版本（需要 llvm-mingw）
LLVM_MINGW_CLANG="/opt/homebrew/opt/llvm-mingw-20250114-ucrt-macos-universal/bin/aarch64-w64-mingw32-clang++"
if [ -x "$LLVM_MINGW_CLANG" ]; then
    echo "Building ARM64 version (llvm-mingw)..."
    mkdir -p build/arm64
    cd build/arm64
    cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-mingw-arm64.cmake \
             -DCMAKE_BUILD_TYPE=Release \
             -DCLAWDESK_ENABLE_OPENSSL=OFF \
             -DCLAWDESK_BUILD_TESTS=OFF
    make -j$(sysctl -n hw.ncpu)
    cd ../..
else
    echo "ARM64 compiler not available ($LLVM_MINGW_CLANG), skipping ARM64 build"
fi

# 输出构建结果
echo ""
echo "Build complete:"
echo "x64:"
ls -lh build/x64/WinBridgeAgent.exe 2>/dev/null || echo "  Build failed"
echo "x86:"
ls -lh build/x86/WinBridgeAgent.exe 2>/dev/null || echo "  Build failed"
if [ -d "build/arm64" ]; then
    echo "ARM64:"
    ls -lh build/arm64/WinBridgeAgent.exe 2>/dev/null || echo "  Build failed"
fi

# 定义部署目标配置
# 格式: "电脑名称|挂载点|IP地址|端口"
DEPLOY_TARGETS=(
    "Test|/Volumes/Test|192.168.31.3|35182"
    # 添加更多电脑配置，例如:
    # "Office|/Volumes/Office|192.168.31.4|35182"
    # "Home|/Volumes/Home|192.168.31.5|35182"
)

# 函数: 部署到指定目标
deploy_to_target() {
    local PC_NAME=$1
    local MOUNT_POINT=$2
    local IP_ADDRESS=$3
    local PORT=$4
    
    echo ""
    echo "========================================"
    echo "Deploying to: $PC_NAME ($IP_ADDRESS:$PORT)"
    echo "========================================"
    
    if [ ! -d "$MOUNT_POINT" ]; then
        echo "⚠ $MOUNT_POINT not found, skipping $PC_NAME"
        return
    fi
    
    # 检测远程服务器上的程序是否在运行
    echo "Checking if WinBridgeAgent is running on $IP_ADDRESS..."
    
    # 尝试访问健康检查端点
    if curl -s --connect-timeout 2 http://$IP_ADDRESS:$PORT/health > /dev/null 2>&1; then
        echo "✓ Server is running, sending exit command..."
        # 尝试从已部署的 config.json 读取 token（兼容旧目录名）
        AUTH_TOKEN=""
        CONFIG_PATH=""
        if [ -f "$MOUNT_POINT/WinBridgeAgent/config.json" ]; then
            CONFIG_PATH="$MOUNT_POINT/WinBridgeAgent/config.json"
        elif [ -f "$MOUNT_POINT/ClawDeskMCP/config.json" ]; then
            CONFIG_PATH="$MOUNT_POINT/ClawDeskMCP/config.json"
        fi

        if [ -n "$CONFIG_PATH" ]; then
            AUTH_TOKEN=$(python3 - <<PY
import json
try:
    with open("$CONFIG_PATH","r",encoding="utf-8") as f:
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
            curl -s -H "Authorization: Bearer $AUTH_TOKEN" http://$IP_ADDRESS:$PORT/exit > /dev/null 2>&1
        else
            curl -s http://$IP_ADDRESS:$PORT/exit > /dev/null 2>&1
        fi
        
        # 等待服务器关闭
        echo "  Waiting for server to shutdown..."
        sleep 2
        
        # 验证服务器已关闭
        for i in 1 2 3 4 5; do
            if curl -s --connect-timeout 2 http://$IP_ADDRESS:$PORT/health > /dev/null 2>&1; then
                echo "  Still running... (${i}/5)"
                sleep 2
            else
                echo "✓ Server shutdown complete"
                break
            fi
        done
    else
        echo "✓ Server is not running"
    fi
    
    mkdir -p $MOUNT_POINT/WinBridgeAgent
    
    if [ -f "build/x64/WinBridgeAgent.exe" ]; then
        cp build/x64/WinBridgeAgent.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgent-x64.exe
        echo "✓ Copied x64 version"
    fi

    if [ -f "build/x64/updater/Updater.exe" ]; then
        cp build/x64/updater/Updater.exe $MOUNT_POINT/WinBridgeAgent/Updater-x64.exe
        echo "✓ Copied x64 updater"
    fi
    
    if [ -f "build/x64/WinBridgeAgentDaemon.exe" ]; then
        cp build/x64/WinBridgeAgentDaemon.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgentDaemon-x64.exe
        echo "✓ Copied x64 daemon"
    fi
    
    if [ -f "build/x86/WinBridgeAgent.exe" ]; then
        cp build/x86/WinBridgeAgent.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgent-x86.exe
        echo "✓ Copied x86 version"
    fi

    if [ -f "build/x86/updater/Updater.exe" ]; then
        cp build/x86/updater/Updater.exe $MOUNT_POINT/WinBridgeAgent/Updater-x86.exe
        echo "✓ Copied x86 updater"
    fi
    
    if [ -f "build/x86/WinBridgeAgentDaemon.exe" ]; then
        cp build/x86/WinBridgeAgentDaemon.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgentDaemon-x86.exe
        echo "✓ Copied x86 daemon"
    fi
    
    if [ -f "build/arm64/WinBridgeAgent.exe" ]; then
        cp build/arm64/WinBridgeAgent.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgent-arm64.exe
        echo "✓ Copied ARM64 version"
    fi

    if [ -f "build/arm64/updater/Updater.exe" ]; then
        cp build/arm64/updater/Updater.exe $MOUNT_POINT/WinBridgeAgent/Updater-arm64.exe
        echo "✓ Copied ARM64 updater"
    fi
    
    if [ -f "build/arm64/WinBridgeAgentDaemon.exe" ]; then
        cp build/arm64/WinBridgeAgentDaemon.exe $MOUNT_POINT/WinBridgeAgent/WinBridgeAgentDaemon-arm64.exe
        echo "✓ Copied ARM64 daemon"
    fi
    
    # 复制配置模板
    if [ -f "resources/config.template.json" ]; then
        cp resources/config.template.json $MOUNT_POINT/WinBridgeAgent/
        echo "✓ Copied config template"
    fi

    # 复制翻译文件
    if [ -f "resources/translations.json" ]; then
        mkdir -p $MOUNT_POINT/WinBridgeAgent/resources
        cp resources/translations.json $MOUNT_POINT/WinBridgeAgent/resources/
        echo "✓ Copied translations.json"
    fi
    
    # 复制 README
    if [ -f "README.md" ]; then
        cp README.md $MOUNT_POINT/WinBridgeAgent/
        echo "✓ Copied README.md"
    fi
    
    # 复制防火墙文档
    if [ -f "FIREWALL.md" ]; then
        cp FIREWALL.md $MOUNT_POINT/WinBridgeAgent/
        echo "✓ Copied FIREWALL.md"
    fi
    
    echo ""
    echo "✓ Files deployed to: $MOUNT_POINT/WinBridgeAgent/ ($PC_NAME)"
    ls -lh $MOUNT_POINT/WinBridgeAgent/
}

# 遍历所有部署目标
for target in "${DEPLOY_TARGETS[@]}"; do
    IFS='|' read -r PC_NAME MOUNT_POINT IP_ADDRESS PORT <<< "$target"
    deploy_to_target "$PC_NAME" "$MOUNT_POINT" "$IP_ADDRESS" "$PORT"
done

echo ""
echo "========================================"
echo "Deployment complete!"
echo "========================================"
