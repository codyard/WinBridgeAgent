# 进程关系说明（主程序 / 守护进程 / 升级器）

本文说明三个可执行文件的职责、启动/退出顺序，以及它们之间的信号和约束。

## 进程角色

1. `ClawDeskMCP.exe`（主程序）
- 提供托盘、HTTP 服务、Dashboard、设置等核心功能。
- 运行时持有互斥体 `ClawDeskMCPServer_SingleInstance`。
- 启动时会按需拉起守护进程（仅当 `daemon_enabled=true`）。

2. `ClawDeskMCPDaemon.exe`（守护进程，隐藏）
- 常驻后台，无 UI。
- 监测主程序是否存在（通过互斥体）。
- 若主程序退出，等待 10 秒再尝试重启。
- 如果配置 `daemon_enabled=false`，守护进程自动退出。
- 接收到退出事件 `ClawDeskMCPDaemon_Exit` 时立即退出（用于更新与手动禁用）。

3. `Updater.exe`（升级器）
- 负责下载后的替换与重启。
- 由主程序在用户确认更新后启动。
- 更新开始前主程序会通知守护进程退出，避免被守护误拉起。

## 启动关系

- 用户启动 `ClawDeskMCP.exe`。
- 主程序读取 `config.json`：
  - 若 `daemon_enabled=true` 且守护进程未运行 → 拉起 `ClawDeskMCPDaemon.exe`。
  - 若 `daemon_enabled=false` → 不拉起守护进程。

## 退出关系

### 手动退出（托盘 Exit）
- 主程序：
  - 将 `daemon_enabled` 设为 `false` 并保存配置。
  - 发送 `ClawDeskMCPDaemon_Exit` 事件让守护进程退出。
  - 自身退出。
- 守护进程：
  - 检测到配置禁用或退出事件后立即退出。

### 非手动退出（异常/崩溃）
- 守护进程：
  - 监测不到主程序互斥体 → 等待 10 秒 → 尝试重启主程序。

## 更新关系

- 主程序发起更新并准备启动 `Updater.exe`：
  - 先发送 `ClawDeskMCPDaemon_Exit`，确保守护进程退出。
  - 启动 `Updater.exe`。
  - 主程序退出，由升级器替换并重启。

## 关键配置与信号

- `config.json`：
  - `daemon_enabled`（默认 `true`）
- 互斥体：
  - 主程序：`ClawDeskMCPServer_SingleInstance`
  - 守护进程：`ClawDeskMCPDaemon_SingleInstance`
- 事件：
  - 守护退出事件：`ClawDeskMCPDaemon_Exit`

## 典型时序（简化）

1. 正常启动
- 主程序启动 → 拉起守护 → 服务运行

2. 用户退出
- 用户点击 Exit → 禁用守护并发送退出事件 → 主程序退出 → 守护退出

3. 异常崩溃
- 主程序崩溃 → 守护检测不到互斥体 → 等待 10 秒 → 重启主程序

4. 更新
- 用户确认更新 → 主程序通知守护退出 → 启动升级器 → 主程序退出 → 升级器完成替换并重启
