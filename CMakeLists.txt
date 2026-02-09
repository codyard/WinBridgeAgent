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
project(WinBridgeAgent VERSION 0.8.105 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# --- Options ---
option(CLAWDESK_ENABLE_OPENSSL "Enable OpenSSL for HTTPS" ON)
option(CLAWDESK_BUILD_TESTS "Build unit tests (native, runnable binaries)" ON)


# --- Source Files ---
# 将所有共享的源文件收集到一个库中，以便主程序和测试程序共享
file(GLOB_RECURSE CLAWDESK_LIB_SOURCES 
    CONFIGURE_DEPENDS
    "src/*.cpp"
)
# 从库中排除特定于主程序和守护进程的 main 函数
list(REMOVE_ITEM CLAWDESK_LIB_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/daemon_main.cpp"
)

add_library(clawdesk_lib STATIC ${CLAWDESK_LIB_SOURCES})

# --- Targets ---
# 主程序
add_executable(WinBridgeAgent src/main.cpp)
target_link_libraries(WinBridgeAgent PRIVATE clawdesk_lib)

# 守护进程（最小依赖，不链接 clawdesk_lib）
add_executable(WinBridgeAgentDaemon WIN32 src/daemon_main.cpp)
target_sources(WinBridgeAgentDaemon PRIVATE
    src/support/config_manager.cpp
    src/utils/log_path.cpp
)

# Updater 子项目
add_subdirectory(updater)


# --- Target-specific Configurations ---

# 配置 clawdesk_lib
target_include_directories(clawdesk_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann-json/include"
)
target_compile_definitions(clawdesk_lib PRIVATE 
    CLAWDESK_VERSION="${PROJECT_VERSION}"
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)
if(CLAWDESK_ENABLE_OPENSSL)
    target_compile_definitions(clawdesk_lib PRIVATE CLAWDESK_OPENSSL_ENABLED)
endif()


# 配置主程序 WinBridgeAgent
target_compile_definitions(WinBridgeAgent PRIVATE 
    CLAWDESK_VERSION="${PROJECT_VERSION}"
)
if(WIN32)
    target_sources(WinBridgeAgent PRIVATE resources/settings_window.rc)
    target_link_libraries(WinBridgeAgent PRIVATE
        ws2_32 gdi32 gdiplus user32 shell32 comdlg32 advapi32 ole32 uuid shlwapi 
        psapi powrprof comctl32 crypt32 dbghelp winhttp
    )
    if(CLAWDESK_ENABLE_OPENSSL)
        target_link_libraries(WinBridgeAgent PRIVATE ssl crypto)
    endif()
endif()


# 配置守护进程 WinBridgeAgentDaemon（需要自己的 include 路径，因为不链接 clawdesk_lib）
target_include_directories(WinBridgeAgentDaemon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann-json/include"
)
target_compile_definitions(WinBridgeAgentDaemon PRIVATE 
    CLAWDESK_VERSION="${PROJECT_VERSION}"
)
if(WIN32)
    target_link_libraries(WinBridgeAgentDaemon PRIVATE
        ws2_32 gdi32 gdiplus user32 shell32 comdlg32 advapi32 ole32 uuid shlwapi 
        psapi powrprof comctl32 crypt32
    )
    if(CLAWDESK_ENABLE_OPENSSL)
        target_link_libraries(WinBridgeAgentDaemon PRIVATE ssl crypto)
    endif()
endif()


# --- Build Type Configurations ---
if(MINGW)
    # 静态链接运行时（GCC 用 -static-libgcc/-static-libstdc++，Clang/llvm-mingw 忽略它们但 -static 生效）
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(STATIC_LINK_FLAGS -static-libgcc -static-libstdc++ -static)
    else()
        set(STATIC_LINK_FLAGS -static)
    endif()
    target_link_options(WinBridgeAgent PRIVATE ${STATIC_LINK_FLAGS})
    target_link_options(WinBridgeAgentDaemon PRIVATE ${STATIC_LINK_FLAGS})
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MINGW)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -s")
        else()
            # Clang: -O3 有效, -s 由 llvm strip 处理
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
        endif()
        set_target_properties(WinBridgeAgent PROPERTIES WIN32_EXECUTABLE TRUE)
        set_target_properties(WinBridgeAgentDaemon PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

# --- Testing ---
if(CLAWDESK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
