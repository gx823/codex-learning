from __future__ import annotations

import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PYTHON = ROOT / "ltx_env" / "Scripts" / "python.exe"
LTX_ROOT = ROOT / "LTX-Video"
INFERENCE = LTX_ROOT / "inference.py"
CONFIG = LTX_ROOT / "configs" / "ltxv-2b-0.9.8-distilled-lowvram.yaml"
REFS = ROOT / "runway_refs"
OUT = ROOT / "ltx_clips"


CLIPS = [
    {
        "name": "01_miku_confession",
        "seed": 27417,
        "num_frames": 121,
        "images": [REFS / "start.png", REFS / "mid.png"],
        "frames": [0, 112],
        "prompt": (
            "A continuous hand-drawn Japanese romance anime shot under blooming cherry trees. "
            "The same black-haired schoolgirl in a seafoam-green blazer faces the same black-haired schoolboy. "
            "She blinks twice, takes a small nervous breath, her shoulders rise and settle, her cheeks gradually blush, "
            "then her eyes become determined. Her lips and jaw move naturally as she speaks a heartfelt Japanese confession. "
            "She lifts her right hand from her chest, reaches toward him, and gently catches his sleeve. "
            "The boy reacts with a brief surprised blink and a slight backward head movement, then watches her intently. "
            "Hair tips, blazer hems, and pink petals move in a soft spring breeze. Stable faces, expressive eyes, coherent hands, "
            "smooth body motion, subtle handheld camera push-in, cinematic soft daylight."
        ),
    },
    {
        "name": "02_yukiya_reply_v2",
        "seed": 11092,
        "num_frames": 121,
        "images": [REFS / "start.png", REFS / "final.png"],
        "frames": [0, 112],
        "prompt": (
            "A continuous hand-drawn Japanese romance anime shot with the same two students and exact seafoam-green uniforms. "
            "The boy looks startled for one beat, his eyebrows soften, then he exhales and smiles with quiet relief. "
            "His lips, jaw, and cheeks move naturally while he gives a sincere Japanese reply. He steps half a pace closer, "
            "raises his left hand, gently covers the girl's reaching hand, and interlaces their fingers without sudden jumps. "
            "The girl looks up, blinks, and her nervous expression changes into a warm tearful smile. Their shoulders and torsos "
            "shift with the step and hand movement. Hair strands, sleeves, and cherry petals sway continuously. Stable facial identity, "
            "clear eye highlights, correct hands, smooth romantic animation, slow camera arc, soft spring backlight."
        ),
    },
    {
        "name": "03_together",
        "seed": 48126,
        "num_frames": 129,
        "images": [REFS / "final.png"],
        "frames": [0],
        "prompt": (
            "A continuous close romantic hand-drawn Japanese anime shot of the same black-haired schoolboy and schoolgirl under cherry blossoms. "
            "They keep holding hands, exchange a shy glance, and each gives a small natural speaking mouth movement for one short line. "
            "The girl lets out a tiny relieved laugh, her eyes narrow into a smile, and one tear glints at the corner of her eye. "
            "The boy smiles more openly, gently leans forward, and the girl leans in too until their foreheads touch. "
            "Both close their eyes and breathe softly; their shoulders rise and fall slightly. Fine hair strands and uniform collars "
            "move in the breeze while petals pass in front of and behind them. Stable detailed faces, coherent fingers, continuous body motion, "
            "no frozen pose, slow intimate camera push-in, warm pink sunset light."
        ),
    },
]


def run_clip(clip: dict) -> None:
    output_dir = OUT / clip["name"]
    output_dir.mkdir(parents=True, exist_ok=True)
    command = [
        str(PYTHON),
        str(INFERENCE),
        "--prompt",
        clip["prompt"],
        "--negative_prompt",
        (
            "still image, slideshow, frozen character, no body motion, locked face, bad lip sync, "
            "worst quality, inconsistent motion, jitter, flicker, distorted face, extra fingers, broken hands, "
            "identity change, wrong clothes, live action, photorealistic, subtitles, text, logo"
        ),
        "--pipeline_config",
        str(CONFIG),
        "--conditioning_media_paths",
        *[str(path) for path in clip["images"]],
        "--conditioning_start_frames",
        *[str(frame) for frame in clip["frames"]],
        "--conditioning_strengths",
        *(["1.0"] * len(clip["images"])),
        "--height",
        "448",
        "--width",
        "704",
        "--num_frames",
        str(clip["num_frames"]),
        "--frame_rate",
        "24",
        "--seed",
        str(clip["seed"]),
        "--image_cond_noise_scale",
        "0.08",
        "--offload_to_cpu",
        "--output_path",
        str(output_dir),
    ]
    print(f"Generating {clip['name']}...", flush=True)
    subprocess.run(command, cwd=LTX_ROOT, check=True, env=os.environ.copy())


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for clip in CLIPS:
        run_clip(clip)


if __name__ == "__main__":
    main()
