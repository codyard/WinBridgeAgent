# 在 macOS 部署 Qwen3-TTS 并提供 WebAPI

目标：在 macOS 上运行一个常驻的本地 TTS 服务，通过 HTTP API 给其它应用调用。

本文用到的模型/SDK来自 `QwenLM/Qwen3-TTS`，其 Python 用法示例包含：
- `Qwen3TTSModel.from_pretrained(...)`
- `generate_custom_voice(text=..., speaker=...)`
- `get_supported_speakers()`

（这些接口在官方 README 的 Quickstart 里有示例。）

## 方案选择

- **推荐：FastAPI（REST 风格）**：适合你说的“其它应用通过 webapi 请求”。本文默认用此方案。
- **可选：官方 Gradio Web UI**：适合人工测试/试听，但对“其它应用调用”不如 REST 顺手。

## 1) 创建 Python 环境（venv 示例）

```bash
python3 -m venv ~/venvs/qwen3-tts
source ~/venvs/qwen3-tts/bin/activate
python -m pip install -U pip
```

## 2) 安装依赖

```bash
pip install -r scripts/qwen3_tts_webapi/requirements.txt
```

### 安装 PyTorch（macOS）

Qwen3-TTS 依赖 PyTorch。macOS 上通常推荐直接按 PyTorch 官网指引安装。

你也可以先试：

```bash
pip install torch
```

（Apple Silicon 一般可用 MPS；Intel Mac 只能走 CPU。）

### 安装 Qwen3-TTS（两种方式）

方式 A：从源码安装（更稳，推荐）

```bash
git clone https://github.com/QwenLM/Qwen3-TTS
cd Qwen3-TTS
pip install -e .
```

方式 B：如果官方已发布 PyPI 包

```bash
pip install qwen-tts
```

首次启动会自动下载模型权重（体积较大），请确保网络能访问 HuggingFace。

## 3) 启动 WebAPI 服务

默认只监听本机：

```bash
python scripts/qwen3_tts_webapi/server.py --host 127.0.0.1 --port 8088
```

如果要给局域网其它设备调用：

```bash
python scripts/qwen3_tts_webapi/server.py --host 0.0.0.0 --port 8088
```

## 4) 调用示例

健康检查：

```bash
curl http://127.0.0.1:8088/healthz
```

列出可用音色：

```bash
curl http://127.0.0.1:8088/v1/voices
```

合成并保存 WAV：

```bash
curl -X POST http://127.0.0.1:8088/v1/audio/speech \
  -H 'Content-Type: application/json' \
  -d '{"input":"你好，我是 Qwen3 TTS。","voice":"Cherry","response_format":"wav"}' \
  --output out.wav
```

返回 base64（方便某些前端/跨语言客户端）：

```bash
curl -X POST http://127.0.0.1:8088/v1/audio/speech_base64 \
  -H 'Content-Type: application/json' \
  -d '{"input":"Hello from macOS.","voice":"Cherry"}'
```

## 5) macOS 性能/兼容性说明（重要）

- 官方 README 里示例偏向 CUDA（Linux/NVIDIA）。**macOS 没有 CUDA**。
- 本服务脚本默认逻辑：
  - Apple Silicon 且 `MPS` 可用 → 用 `mps + float16`
  - 否则 → `cpu + float32`
- 你可以用环境变量强制：
  - `QWEN3_TTS_DEVICE=cpu` 或 `QWEN3_TTS_DEVICE=mps`
  - `QWEN3_TTS_DTYPE=float16|float32|bfloat16`（不保证每种组合都可用）

示例：

```bash
export QWEN3_TTS_DEVICE=mps
export QWEN3_TTS_DTYPE=float16
python scripts/qwen3_tts_webapi/server.py --host 127.0.0.1 --port 8088
```

## 6) 常驻运行（launchd 提示）

你可以把启动命令做成 `launchd` 的 LaunchAgent/LaunchDaemon（按需选择是否开机启动）。
建议先手动跑通 API 和模型下载，再做常驻。

一个常见做法是在 `~/Library/LaunchAgents/` 放一个 plist，例如（按需改路径）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>Label</key>
    <string>com.local.qwen3-tts</string>

    <key>ProgramArguments</key>
    <array>
      <string>/Users/yourname/venvs/qwen3-tts/bin/python</string>
      <string>/Volumes/SN770/Downloads/Dev/2026/Products/WinAgent/scripts/qwen3_tts_webapi/server.py</string>
      <string>--host</string>
      <string>127.0.0.1</string>
      <string>--port</string>
      <string>8088</string>
    </array>

    <key>EnvironmentVariables</key>
    <dict>
      <key>QWEN3_TTS_DEVICE</key>
      <string>mps</string>
      <key>QWEN3_TTS_DTYPE</key>
      <string>float16</string>
    </dict>

    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>

    <key>StandardOutPath</key>
    <string>/tmp/qwen3-tts.out.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/qwen3-tts.err.log</string>
  </dict>
</plist>
```

加载/卸载：

```bash
launchctl load -w ~/Library/LaunchAgents/com.local.qwen3-tts.plist
launchctl unload -w ~/Library/LaunchAgents/com.local.qwen3-tts.plist
```
