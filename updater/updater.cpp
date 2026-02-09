#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <shlwapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "shlwapi.lib")

// 升级器版本
#define UPDATER_VERSION "1.0.0"

// 日志函数
void Log(const std::string& message) {
    std::ofstream log("updater.log", std::ios::app);
    if (log.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timestamp[64];
        sprintf_s(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        log << timestamp << message << std::endl;
        log.close();
    }
}

// 等待进程退出
bool WaitForProcessExit(const std::string& processName, int timeoutSeconds) {
    Log("Waiting for process to exit: " + processName);
    
    for (int i = 0; i < timeoutSeconds; ++i) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            continue;
        }
        
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        
        bool found = false;
        if (Process32First(snapshot, &pe32)) {
            do {
                std::string exeName = pe32.szExeFile;
                if (exeName == processName) {
                    found = true;
                    break;
                }
            } while (Process32Next(snapshot, &pe32));
        }
        
        CloseHandle(snapshot);
        
        if (!found) {
            Log("Process exited: " + processName);
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    Log("Timeout waiting for process: " + processName);
    return false;
}

// 备份文件
bool BackupFile(const std::string& source, const std::string& backup) {
    Log("Backing up: " + source + " -> " + backup);
    
    // 创建备份目录
    std::string backupDir = backup.substr(0, backup.find_last_of("\\/"));
    CreateDirectoryA(backupDir.c_str(), NULL);
    
    if (CopyFileA(source.c_str(), backup.c_str(), FALSE)) {
        Log("Backup successful");
        return true;
    } else {
        DWORD error = GetLastError();
        Log("Backup failed, error: " + std::to_string(error));
        return false;
    }
}

// 替换文件
bool ReplaceFile(const std::string& newFile, const std::string& target) {
    Log("Replacing: " + target + " with " + newFile);
    
    // 删除旧文件
    if (PathFileExistsA(target.c_str())) {
        if (!DeleteFileA(target.c_str())) {
            DWORD error = GetLastError();
            Log("Failed to delete old file, error: " + std::to_string(error));
            return false;
        }
    }
    
    // 移动新文件
    if (MoveFileA(newFile.c_str(), target.c_str())) {
        Log("Replace successful");
        return true;
    } else {
        DWORD error = GetLastError();
        Log("Replace failed, error: " + std::to_string(error));
        return false;
    }
}

// 回滚
bool Rollback(const std::string& backup, const std::string& target) {
    Log("Rolling back: " + backup + " -> " + target);
    
    if (PathFileExistsA(backup.c_str())) {
        if (CopyFileA(backup.c_str(), target.c_str(), FALSE)) {
            Log("Rollback successful");
            return true;
        } else {
            DWORD error = GetLastError();
            Log("Rollback failed, error: " + std::to_string(error));
            return false;
        }
    } else {
        Log("Backup file not found: " + backup);
        return false;
    }
}

// 启动程序
bool StartProcess(const std::string& exePath) {
    Log("Starting process: " + exePath);
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (CreateProcessA(
        exePath.c_str(),
        NULL,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Log("Process started successfully");
        return true;
    } else {
        DWORD error = GetLastError();
        Log("Failed to start process, error: " + std::to_string(error));
        return false;
    }
}

// 清理临时文件
void Cleanup(const std::vector<std::string>& files) {
    Log("Cleaning up temporary files");
    
    for (const auto& file : files) {
        if (PathFileExistsA(file.c_str())) {
            if (DeleteFileA(file.c_str())) {
                Log("Deleted: " + file);
            } else {
                Log("Failed to delete: " + file);
            }
        }
    }
}

// 显示进度窗口
HWND CreateProgressWindow() {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "UpdaterProgressClass";
    RegisterClassA(&wc);
    
    HWND hwnd = CreateWindowExA(
        0,
        "UpdaterProgressClass",
        "Updating ClawDesk MCP Server...",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 150,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (hwnd) {
        // 居中显示
        RECT rect;
        GetWindowRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hwnd, NULL,
                    (screenWidth - width) / 2,
                    (screenHeight - height) / 2,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        // 创建状态标签
        CreateWindowA("STATIC", "Please wait...",
                     WS_CHILD | WS_VISIBLE | SS_CENTER,
                     10, 50, 360, 30,
                     hwnd, NULL, GetModuleHandle(NULL), NULL);
        
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    
    return hwnd;
}

// 更新进度窗口文本
void UpdateProgressText(HWND hwnd, const std::string& text) {
    if (hwnd) {
        HWND label = GetWindow(hwnd, GW_CHILD);
        if (label) {
            SetWindowTextA(label, text.c_str());
            UpdateWindow(hwnd);
        }
    }
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 解析命令行参数
    // 格式：updater.exe <new_exe_path> <target_exe_path> <process_name>
    
    std::string cmdLine = lpCmdLine;
    std::vector<std::string> args;
    
    // 简单的参数解析
    size_t pos = 0;
    while (pos < cmdLine.length()) {
        // 跳过空格
        while (pos < cmdLine.length() && cmdLine[pos] == ' ') pos++;
        if (pos >= cmdLine.length()) break;
        
        std::string arg;
        if (cmdLine[pos] == '"') {
            // 引号包围的参数
            pos++;
            while (pos < cmdLine.length() && cmdLine[pos] != '"') {
                arg += cmdLine[pos++];
            }
            pos++; // 跳过结束引号
        } else {
            // 普通参数
            while (pos < cmdLine.length() && cmdLine[pos] != ' ') {
                arg += cmdLine[pos++];
            }
        }
        args.push_back(arg);
    }
    
    if (args.size() < 3) {
        MessageBoxA(NULL, 
                   "Usage: updater.exe <new_exe_path> <target_exe_path> <process_name>",
                   "Updater Error",
                   MB_OK | MB_ICONERROR);
        return 1;
    }
    
    std::string newExePath = args[0];
    std::string targetExePath = args[1];
    std::string processName = args[2];
    
    Log("=== ClawDesk MCP Updater v" UPDATER_VERSION " ===");
    Log("New file: " + newExePath);
    Log("Target file: " + targetExePath);
    Log("Process name: " + processName);
    
    // 创建进度窗口
    HWND progressWnd = CreateProgressWindow();
    
    // 1. 等待主程序退出
    UpdateProgressText(progressWnd, "Waiting for application to close...");
    if (!WaitForProcessExit(processName, 30)) {
        Log("Failed to wait for process exit");
        MessageBoxA(NULL,
                   "Failed to close the application. Please close it manually and try again.",
                   "Update Failed",
                   MB_OK | MB_ICONERROR);
        if (progressWnd) DestroyWindow(progressWnd);
        return 1;
    }
    
    // 等待额外 1 秒确保文件句柄释放
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 2. 备份旧版本
    UpdateProgressText(progressWnd, "Backing up current version...");
    std::string backupPath = targetExePath + ".backup";
    if (!BackupFile(targetExePath, backupPath)) {
        Log("Failed to backup old version");
        MessageBoxA(NULL,
                   "Failed to backup the current version.",
                   "Update Failed",
                   MB_OK | MB_ICONERROR);
        if (progressWnd) DestroyWindow(progressWnd);
        return 1;
    }
    
    // 3. 替换文件
    UpdateProgressText(progressWnd, "Installing new version...");
    if (!ReplaceFile(newExePath, targetExePath)) {
        Log("Failed to replace file, attempting rollback");
        
        // 回滚
        UpdateProgressText(progressWnd, "Rolling back...");
        if (Rollback(backupPath, targetExePath)) {
            Log("Rollback successful");
            MessageBoxA(NULL,
                       "Update failed. The application has been restored to the previous version.",
                       "Update Failed",
                       MB_OK | MB_ICONWARNING);
        } else {
            Log("Rollback failed");
            MessageBoxA(NULL,
                       "Update failed and rollback failed. Please reinstall the application.",
                       "Critical Error",
                       MB_OK | MB_ICONERROR);
        }
        
        if (progressWnd) DestroyWindow(progressWnd);
        return 1;
    }
    
    // 4. 启动新版本
    UpdateProgressText(progressWnd, "Starting new version...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!StartProcess(targetExePath)) {
        Log("Failed to start new version");
        MessageBoxA(NULL,
                   "Update completed but failed to start the application. Please start it manually.",
                   "Update Warning",
                   MB_OK | MB_ICONWARNING);
    } else {
        Log("Update completed successfully");
    }
    
    // 5. 清理
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (progressWnd) DestroyWindow(progressWnd);
    
    // 清理临时文件（备份文件保留一段时间）
    std::vector<std::string> tempFiles;
    // 可以添加其他临时文件
    Cleanup(tempFiles);
    
    Log("Updater finished");
    
    return 0;
}
