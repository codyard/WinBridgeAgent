import argparse
import asyncio
import base64
import io
import os
from typing import Optional

import soundfile as sf
import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import JSONResponse, Response
from pydantic import BaseModel, Field

try:
    import torch
except Exception as exc:  # pragma: no cover
    raise SystemExit(
        "Missing dependency 'torch'. Install PyTorch first, then retry."
    ) from exc

try:
    from qwen_tts import Qwen3TTSModel
except Exception as exc:  # pragma: no cover
    raise SystemExit(
        "Missing dependency 'qwen_tts'. Install Qwen3-TTS SDK (pip install qwen-tts or pip install -e Qwen3-TTS)."
    ) from exc


def _pick_device(device: Optional[str]) -> str:
    if device:
        return device
    if torch.backends.mps.is_available():
        return "mps"
    return "cpu"


def _pick_dtype(dtype: Optional[str], device: str) -> torch.dtype:
    if dtype:
        try:
            return getattr(torch, dtype)
        except AttributeError as exc:
            raise ValueError(f"Unsupported torch dtype: {dtype}") from exc
    if device == "mps":
        return torch.float16
    return torch.float32


class SpeechRequest(BaseModel):
    input: str = Field(..., description="Text to synthesize")
    voice: str = Field("Cherry", description="Speaker name")
    model: Optional[str] = Field(
        None,
        description=(
            "Model ID or preset name. Default uses Qwen/Qwen3-TTS-0.6B-CustomVoice"
        ),
    )
    response_format: str = Field(
        "wav",
        description="Only 'wav' is supported for now",
    )


class SpeechBase64Response(BaseModel):
    sample_rate: int
    audio_b64: str


def _wav_to_bytes(wav, sample_rate: int) -> bytes:
    buf = io.BytesIO()
    sf.write(buf, wav, sample_rate, format="WAV")
    return buf.getvalue()


def _default_model_id() -> str:
    return os.environ.get("QWEN3_TTS_MODEL_ID", "Qwen/Qwen3-TTS-0.6B-CustomVoice")


def create_app() -> FastAPI:
    app = FastAPI(title="Qwen3-TTS Web API")

    state = {
        "model": None,
        "lock": asyncio.Lock(),
        "device": None,
        "dtype": None,
        "model_id": None,
    }

    @app.on_event("startup")
    def _startup() -> None:
        device = _pick_device(os.environ.get("QWEN3_TTS_DEVICE"))
        dtype = _pick_dtype(os.environ.get("QWEN3_TTS_DTYPE"), device)
        model_id = _default_model_id()

        state["device"] = device
        state["dtype"] = dtype
        state["model_id"] = model_id

        state["model"] = Qwen3TTSModel.from_pretrained(
            model_id,
            torch_dtype=dtype,
            device_map=device,
        )

    @app.get("/healthz")
    def healthz():
        return {
            "ok": True,
            "device": state["device"],
            "dtype": str(state["dtype"]),
            "model_id": state["model_id"],
        }

    @app.get("/v1/voices")
    def voices():
        if not state["model"]:
            raise HTTPException(status_code=503, detail="Model not loaded")
        return {"voices": state["model"].get_supported_speakers()}

    @app.post("/v1/audio/speech")
    async def speech(req: SpeechRequest):
        if req.model and req.model != state["model_id"]:
            raise HTTPException(
                status_code=400,
                detail=(
                    "Per-request model override is not supported. "
                    "Set QWEN3_TTS_MODEL_ID and restart the service."
                ),
            )
        if req.response_format.lower() != "wav":
            raise HTTPException(status_code=400, detail="Only response_format=wav supported")
        if not state["model"]:
            raise HTTPException(status_code=503, detail="Model not loaded")

        async with state["lock"]:
            try:
                wavs, sample_rate = state["model"].generate_custom_voice(
                    text=req.input,
                    speaker=req.voice,
                )
            except Exception as exc:
                raise HTTPException(status_code=500, detail=str(exc)) from exc

        wav_bytes = _wav_to_bytes(wavs[0], sample_rate)
        return Response(content=wav_bytes, media_type="audio/wav")

    @app.post("/v1/audio/speech_base64", response_model=SpeechBase64Response)
    async def speech_base64(req: SpeechRequest):
        if req.model and req.model != state["model_id"]:
            raise HTTPException(
                status_code=400,
                detail=(
                    "Per-request model override is not supported. "
                    "Set QWEN3_TTS_MODEL_ID and restart the service."
                ),
            )
        if not state["model"]:
            raise HTTPException(status_code=503, detail="Model not loaded")

        async with state["lock"]:
            try:
                wavs, sample_rate = state["model"].generate_custom_voice(
                    text=req.input,
                    speaker=req.voice,
                )
            except Exception as exc:
                raise HTTPException(status_code=500, detail=str(exc)) from exc

        wav_bytes = _wav_to_bytes(wavs[0], sample_rate)
        return JSONResponse(
            {
                "sample_rate": sample_rate,
                "audio_b64": base64.b64encode(wav_bytes).decode("utf-8"),
            }
        )

    return app


def main() -> None:
    parser = argparse.ArgumentParser(description="Qwen3-TTS Web API (FastAPI)")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8088)
    args = parser.parse_args()

    app = create_app()
    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
