# 单实例检测机制 - v0.4.0

## 发布日期

2026年2月4日

## 功能概述

实现了单实例检测机制，确保同一时间只能运行一个 ClawDesk MCP Server 实例。当用户尝试启动第二个实例时，系统会自动处理：

1. 检测到已有实例正在运行
2. 向旧实例发送退出命令
3. 等待旧实例优雅退出（最多8秒）
4. 如果旧实例成功退出，新实例继续启动
5. 如果旧实例未能及时退出，显示提示信息并取消启动

## 技术实现

### 核心机制

使用 Windows 互斥锁（Mutex）实现单实例检测：

```cpp
HANDLE instanceMutex = CreateMutexW(NULL, TRUE, L"ClawDeskMCPServer_SingleInstance");
if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    // 已有实例在运行
    HWND existing = FindWindowA("ClawDeskMCPServerClass", NULL);
    if (existing) {
        PostMessage(existing, WM_EXIT_COMMAND, 0, 0);
    }
    DWORD waitResult = WaitForSingleObject(instanceMutex, 8000);
    if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
        MessageBox(NULL,
                   "ClawDesk MCP Server is still running. Please close it first.",
                   "ClawDesk MCP Server",
                   MB_OK | MB_ICONINFORMATION);
        CloseHandle(instanceMutex);
        return 0;
    }
}
```

### 关键特性

1. **全局唯一标识**：使用命名互斥锁 `ClawDeskMCPServer_SingleInstance`
2. **优雅退出机制**：
    - 查找已运行实例的窗口句柄
    - 发送 `WM_EXIT_COMMAND` 消息触发优雅退出
    - 等待最多8秒让旧实例完成清理工作
3. **超时保护**：如果8秒内旧实例未退出，显示友好提示并取消启动
4. **资源清理**：使用 lambda 函数确保所有退出路径都正确释放互斥锁

### 清理机制

```cpp
auto closeInstanceMutex = [&]() {
    if (instanceMutex) {
        CloseHandle(instanceMutex);
        instanceMutex = NULL;
    }
};
```

在所有可能的退出路径上调用 `closeInstanceMutex()`：

- 配置加载失败
- 窗口类注册失败
- 窗口创建失败
- 托盘图标创建失败
- 服务器启动失败
- 正常退出

## 用户体验

### 场景1：正常启动（无已运行实例）

- 程序正常启动，创建互斥锁
- 用户可以正常使用所有功能

### 场景2：尝试启动第二个实例（已有实例运行）

1. 新实例检测到互斥锁已存在
2. 自动向旧实例发送退出命令
3. 等待旧实例退出（最多8秒）
4. 旧实例退出后，新实例继续启动
5. 用户感觉像是"重启"了程序

### 场景3：旧实例无法退出

1. 新实例检测到互斥锁已存在
2. 向旧实例发送退出命令
3. 等待8秒后旧实例仍未退出
4. 显示提示框："ClawDesk MCP Server is still running. Please close it first."
5. 新实例取消启动
6. 用户需要手动关闭旧实例（通过任务管理器或托盘图标）

## 代码位置

**文件**：`src/main.cpp`
**函数**：`WinMain`
**行号**：3287-3309

## 测试建议

### 测试用例1：正常启动

1. 确保没有 ClawDesk MCP Server 实例在运行
2. 启动程序
3. 验证程序正常启动并显示托盘图标

### 测试用例2：双击启动（模拟重启）

1. 启动第一个实例
2. 等待完全启动（托盘图标出现）
3. 再次双击程序图标
4. 验证：
    - 旧实例自动退出
    - 新实例成功启动
    - 整个过程在8秒内完成

### 测试用例3：强制启动（旧实例卡死）

1. 启动第一个实例
2. 使用任务管理器挂起进程（模拟卡死）
3. 尝试启动第二个实例
4. 验证：
    - 等待8秒后显示提示框
    - 提示框内容正确
    - 新实例未启动

### 测试用例4：快速连续启动

1. 快速双击程序图标多次
2. 验证：
    - 只有一个实例最终运行
    - 没有出现多个实例
    - 没有崩溃或错误

## 构建信息

**版本**：0.4.0
**构建日期**：2026年2月4日 19:46

### 校验和

- **x64**：MD5 = e551462b9c0e208db12a795375fd876b (2.0MB)
- **x86**：MD5 = ae576f3b8893d0dc491acdffe5469821 (2.1MB)

## 部署位置

文件已复制到 `/Volumes/Test/ClawDeskMCP/` 用于 Windows 测试：

- `ClawDeskMCP-x64.exe` (2.0MB)
- `ClawDeskMCP-x86.exe` (2.1MB)

## 相关需求

此功能满足以下需求：

- 防止用户意外启动多个实例
- 提供优雅的实例替换机制
- 避免端口冲突和资源竞争
- 改善用户体验（双击图标相当于重启）

## 技术优势

1. **线程安全**：使用 Windows 系统级互斥锁，跨进程可靠
2. **优雅退出**：通过消息机制触发正常退出流程，而非强制终止
3. **超时保护**：避免无限等待，8秒后自动放弃
4. **资源清理**：确保互斥锁在所有情况下都能正确释放
5. **用户友好**：提供清晰的提示信息，不会让用户困惑

## 注意事项

1. **互斥锁名称**：`ClawDeskMCPServer_SingleInstance` 是全局唯一的，不要修改
2. **等待时间**：8秒是合理的超时时间，考虑了正常退出所需的时间
3. **窗口类名**：`ClawDeskMCPServerClass` 必须与窗口注册时使用的类名一致
4. **消息处理**：确保 `WM_EXIT_COMMAND` 消息在窗口过程中正确处理

## 未来改进

可能的改进方向：

1. 添加命令行参数支持强制启动（用于调试）
2. 记录单实例检测事件到审计日志
3. 提供更详细的错误信息（如旧实例的PID）
4. 支持多用户环境（每个用户一个实例）
