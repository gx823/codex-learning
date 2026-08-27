from __future__ import annotations

import os
from pathlib import Path

import numpy as np
import soundfile as sf
from kokoro import KPipeline


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "voices_v3"
OUT.mkdir(parents=True, exist_ok=True)


LINES = [
    (
        "miku_01",
        "ねえ、雪也。もう、この『好きって言ったら負け』のゲーム……終わりにしない？",
        "jf_alpha",
        1.18,
    ),
    (
        "yukiya_01",
        "それ、俺の台詞。ずっと前から……美久が好きだ。",
        "jm_kumo",
        1.10,
    ),
    (
        "miku_02",
        "……じゃあ、引き分け？",
        "jf_alpha",
        1.00,
    ),
    (
        "yukiya_02",
        "いや。二人とも、勝ちだ。",
        "jm_kumo",
        1.02,
    ),
]


def synthesize(pipeline: KPipeline, text: str, voice: str, speed: float) -> np.ndarray:
    chunks = []
    for _, _, audio in pipeline(text, voice=voice, speed=speed):
        chunks.append(audio.detach().cpu().numpy() if hasattr(audio, "detach") else np.asarray(audio))
    if not chunks:
        raise RuntimeError(f"No audio generated for {voice}: {text}")
    silence = np.zeros(int(0.08 * 24000), dtype=np.float32)
    return np.concatenate([silence, *chunks, silence]).astype(np.float32)


def main() -> None:
    os.environ.setdefault("HF_HOME", str(ROOT / "hf_cache"))
    pipeline = KPipeline(lang_code="j", device="cpu")
    for name, text, voice, speed in LINES:
        audio = synthesize(pipeline, text, voice, speed)
        path = OUT / f"{name}.wav"
        sf.write(path, audio, 24000, subtype="PCM_16")
        print(f"{path} ({len(audio) / 24000:.2f}s)", flush=True)


if __name__ == "__main__":
    main()
