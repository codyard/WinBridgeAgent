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

# 一键格式化项目代码
# 依赖: clang-format (brew install clang-format)
# 用法: ./scripts/format.sh [--check]
#   --check  只检查不修改（CI 模式，有差异则返回非零退出码）

set -euo pipefail
cd "$(dirname "$0")/.."

# 检查 clang-format 是否可用
if ! command -v clang-format &>/dev/null; then
    echo "❌ clang-format not found. Install: brew install clang-format"
    exit 1
fi

echo "clang-format version: $(clang-format --version)"

# 收集所有 .cpp/.h 文件（排除 third_party）
FILES=$(find src include -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
COUNT=$(echo "$FILES" | wc -l | tr -d ' ')

if [[ "${1:-}" == "--check" ]]; then
    echo "Checking $COUNT files..."
    DIFF_COUNT=0
    for f in $FILES; do
        if ! clang-format --dry-run -Werror "$f" 2>/dev/null; then
            echo "  ✗ $f"
            DIFF_COUNT=$((DIFF_COUNT + 1))
        fi
    done
    if [[ $DIFF_COUNT -gt 0 ]]; then
        echo ""
        echo "❌ $DIFF_COUNT file(s) need formatting. Run: ./scripts/format.sh"
        exit 1
    else
        echo "✓ All $COUNT files are properly formatted."
        exit 0
    fi
else
    echo "Formatting $COUNT files..."
    for f in $FILES; do
        clang-format -i "$f"
    done
    echo "✓ Done. $COUNT files formatted."
fi
