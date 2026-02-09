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


# Dashboard 测试脚本
# 用于演示 Dashboard 的实时监控功能

echo "================================"
echo "Dashboard 测试脚本"
echo "================================"
echo ""
echo "请确保："
echo "1. ClawDesk MCP Server 正在运行"
echo "2. Dashboard 窗口已打开（右键托盘图标 -> Dashboard）"
echo ""
echo "按回车键开始测试..."
read

SERVER="http://192.168.31.3:35182"

echo ""
echo "测试 1: 健康检查"
echo "--------------------------------"
echo "发送请求: GET $SERVER/health"
curl -s $SERVER/health | jq . 2>/dev/null || curl -s $SERVER/health
echo ""
sleep 2

echo ""
echo "测试 2: 状态查询"
echo "--------------------------------"
echo "发送请求: GET $SERVER/status"
curl -s $SERVER/status | jq . 2>/dev/null || curl -s $SERVER/status
echo ""
sleep 2

echo ""
echo "测试 3: 根路径访问"
echo "--------------------------------"
echo "发送请求: GET $SERVER/"
curl -s $SERVER/ | jq . 2>/dev/null || curl -s $SERVER/
echo ""
sleep 2

echo ""
echo "测试 4: 不存在的端点（会产生错误）"
echo "--------------------------------"
echo "发送请求: GET $SERVER/unknown"
curl -s $SERVER/unknown | jq . 2>/dev/null || curl -s $SERVER/unknown
echo ""
sleep 2

echo ""
echo "测试 5: 连续请求（观察日志滚动）"
echo "--------------------------------"
for i in {1..5}; do
    echo "请求 $i/5: GET $SERVER/health"
    curl -s $SERVER/health > /dev/null
    sleep 0.5
done
echo "完成"
sleep 2

echo ""
echo "================================"
echo "测试完成！"
echo "================================"
echo ""
echo "请查看 Dashboard 窗口，你应该看到："
echo "- [REQ] 标记的请求日志"
echo "- [PRO] 标记的处理日志"
echo "- [OK ] 标记的成功日志"
echo "- [ERR] 标记的错误日志（测试 4）"
echo ""
echo "你可以："
echo "- 点击 'Clear Logs' 清空日志"
echo "- 点击 'Copy All' 复制所有日志"
echo "- 使用鼠标滚轮查看历史日志"
echo ""
