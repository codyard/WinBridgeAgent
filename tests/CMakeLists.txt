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

# 查找所有单元测试源文件
file(GLOB TEST_SOURCES "unit/*.cpp")

# 添加测试可执行文件
add_executable(run_tests ${TEST_SOURCES})

# 链接 clawdesk_lib (业务代码) 和 GoogleTest
target_link_libraries(run_tests PRIVATE
    clawdesk_lib
    GTest::gtest_main
)

# 包含 gtest so it can be found
include(GoogleTest)

# 自动发现测试用例
gtest_discover_tests(run_tests)