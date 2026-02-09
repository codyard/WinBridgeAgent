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
#
# Entry point is WinMain, so this must be a WIN32 subsystem executable on
# Visual Studio / MSVC as well (otherwise the linker looks for main()).
add_executable(Updater WIN32 updater.cpp)

# Windows 特定链接库
if(WIN32)
    # --- VERSIONINFO resource (File Properties -> Details) ---
    set(WBA_COMPANY_NAME "Codyard")
    set(WBA_PRODUCT_NAME "WinBridgeAgent MCP Server")
    set(WBA_LEGAL_COPYRIGHT "Copyright (C) 2026 Codyard")

    # When built as a subdirectory of the main project, prefer its version.
    if(DEFINED WinBridgeAgent_VERSION_MAJOR)
        set(WBA_VERSION_MAJOR ${WinBridgeAgent_VERSION_MAJOR})
        set(WBA_VERSION_MINOR ${WinBridgeAgent_VERSION_MINOR})
        set(WBA_VERSION_PATCH ${WinBridgeAgent_VERSION_PATCH})
        set(WBA_VERSION_STR "${WinBridgeAgent_VERSION}")
    else()
        set(WBA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
        set(WBA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
        set(WBA_VERSION_PATCH ${PROJECT_VERSION_PATCH})
        set(WBA_VERSION_STR "${PROJECT_VERSION}")
    endif()

    set(WBA_FILE_DESCRIPTION "WinBridgeAgent MCP Server Updater")
    set(WBA_INTERNAL_NAME "Updater")
    set(WBA_ORIGINAL_FILENAME "Updater.exe")
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/../resources/version_info.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version_info_updater.rc"
        @ONLY
    )

    target_sources(Updater PRIVATE updater.rc)
    target_sources(Updater PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version_info_updater.rc")
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
