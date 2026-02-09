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

cmake_minimum_required(VERSION 3.20)
project(ClawDeskMCPUpdater VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
add_executable(Updater updater.cpp)

# Windows 特定链接库
if(WIN32)
    target_sources(Updater PRIVATE updater.rc)
    target_link_libraries(Updater
        shlwapi     # Shell API
    )

    # 静态链接 MinGW 运行时
    if(MINGW)
        target_link_options(Updater PRIVATE
            -static-libgcc
            -static-libstdc++
            -static
        )
    endif()
endif()

# Release 优化
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MINGW)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -s")
        set_target_properties(Updater PROPERTIES
            WIN32_EXECUTABLE TRUE  # 无控制台窗口
        )
    endif()
endif()
