from __future__ import annotations

import asyncio
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "deps"))

import edge_tts  # noqa: E402


LINES = [
    (
        "miku_01",
        "ねえ、雪也。もう、この『好きって言ったら負け』のゲーム……終わりにしない？",
        "ja-JP-ShioriNeural",
        "-8%",
        "+5Hz",
    ),
    (
        "yukiya_01",
        "それ、俺の台詞。ずっと前から……美久が好きだ。",
        "ja-JP-NaokiNeural",
        "-7%",
        "-2Hz",
    ),
    (
        "miku_02",
        "……じゃあ、引き分け？",
        "ja-JP-ShioriNeural",
        "-13%",
        "+7Hz",
    ),
    (
        "yukiya_02",
        "いや。二人とも、勝ちだ。",
        "ja-JP-NaokiNeural",
        "-11%",
        "-1Hz",
    ),
]


async def main() -> None:
    out_dir = ROOT / "voices_v3"
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, text, voice, rate, pitch in LINES:
        output = out_dir / f"{name}.mp3"
        communicate = edge_tts.Communicate(
            text=text,
            voice=voice,
            rate=rate,
            pitch=pitch,
            volume="+0%",
        )
        await communicate.save(str(output))
        print(output, flush=True)


if __name__ == "__main__":
    asyncio.run(main())
