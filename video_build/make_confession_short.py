from __future__ import annotations

import math
import os
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont


WIDTH, HEIGHT = 1920, 1080
FPS = 30
DURATION = 20.0
SCENE_LENGTH = 5.0
FADE_LENGTH = 0.72

ROOT = Path(__file__).resolve().parent
STORYBOARD = ROOT / "storyboard.png"
MUSIC = ROOT / "original_ambient.wav"


def versioned_output(folder: Path, stem: str, suffix: str) -> Path:
    candidate = folder / f"{stem}{suffix}"
    if not candidate.exists():
        return candidate
    index = 2
    while True:
        candidate = folder / f"{stem}_v{index}{suffix}"
        if not candidate.exists():
            return candidate
        index += 1


def desktop_folder() -> Path:
    home = Path(os.environ["USERPROFILE"])
    desktop = home / "Desktop"
    desktop.mkdir(parents=True, exist_ok=True)
    return desktop


def make_music(path: Path) -> None:
    sample_rate = 48_000
    count = int(DURATION * sample_rate)
    mix = np.zeros(count, dtype=np.float64)

    # Original four-chord ambient cue: Cmaj7 - Am7 - Fmaj7 - Gadd9.
    chords = [
        [261.63, 329.63, 392.00, 493.88],
        [220.00, 261.63, 329.63, 392.00],
        [174.61, 220.00, 261.63, 329.63],
        [196.00, 246.94, 293.66, 440.00],
    ]
    chord_samples = int(SCENE_LENGTH * sample_rate)
    for chord_index, notes in enumerate(chords):
        start = chord_index * chord_samples
        end = min(start + chord_samples, count)
        t = np.arange(end - start, dtype=np.float64) / sample_rate
        envelope = np.minimum(t / 0.8, 1.0) * np.minimum((SCENE_LENGTH - t) / 0.85, 1.0)
        envelope = np.clip(envelope, 0.0, 1.0)
        pad = np.zeros_like(t)
        for note_index, frequency in enumerate(notes):
            phase = note_index * 0.47
            pad += np.sin(2 * np.pi * frequency * t + phase)
            pad += 0.18 * np.sin(2 * np.pi * frequency * 2 * t + phase / 2)
        mix[start:end] += 0.042 * envelope * pad / len(notes)

    # Soft bell-like notes give the cue a gentle anime-romance sparkle.
    melody = [659.25, 783.99, 987.77, 783.99, 659.25, 523.25, 587.33, 659.25,
              698.46, 880.00, 1046.50, 880.00, 783.99, 659.25, 587.33, 523.25]
    for note_index, frequency in enumerate(melody):
        start_time = 0.55 + note_index * 1.20
        start = int(start_time * sample_rate)
        length = min(int(1.55 * sample_rate), count - start)
        if length <= 0:
            continue
        t = np.arange(length, dtype=np.float64) / sample_rate
        bell = (np.sin(2 * np.pi * frequency * t)
                + 0.32 * np.sin(2 * np.pi * frequency * 2.01 * t)
                + 0.12 * np.sin(2 * np.pi * frequency * 3.98 * t))
        bell *= np.exp(-3.1 * t)
        mix[start:start + length] += 0.048 * bell

    fade = int(1.2 * sample_rate)
    mix[:fade] *= np.linspace(0, 1, fade)
    mix[-fade:] *= np.linspace(1, 0, fade)
    peak = max(float(np.max(np.abs(mix))), 1e-9)
    mix = np.clip(mix / max(peak / 0.35, 1.0), -1.0, 1.0)

    # A tiny stereo delay adds width without using any sampled material.
    delay = int(0.017 * sample_rate)
    left = mix
    right = np.zeros_like(mix)
    right[delay:] = mix[:-delay]
    stereo = np.column_stack((left, right))
    pcm = (stereo * 32767).astype("<i2")
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(pcm.tobytes())


def crop_panels(sheet: Image.Image) -> list[Image.Image]:
    width, height = sheet.size
    gutter_x = width // 2
    gutter_y = height // 2
    margin = 3
    boxes = [
        (0, 0, gutter_x - margin, gutter_y - margin),
        (gutter_x + margin, 0, width, gutter_y - margin),
        (0, gutter_y + margin, gutter_x - margin, height),
        (gutter_x + margin, gutter_y + margin, width, height),
    ]
    panels = []
    for box in boxes:
        panel = sheet.crop(box).convert("RGB")
        source_ratio = panel.width / panel.height
        target_ratio = WIDTH / HEIGHT
        if source_ratio > target_ratio:
            crop_width = int(panel.height * target_ratio)
            left = (panel.width - crop_width) // 2
            panel = panel.crop((left, 0, left + crop_width, panel.height))
        elif source_ratio < target_ratio:
            crop_height = int(panel.width / target_ratio)
            top = (panel.height - crop_height) // 2
            panel = panel.crop((0, top, panel.width, top + crop_height))
        panels.append(panel)
    return panels


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    choices = [
        Path(r"C:\Windows\Fonts\simhei.ttf") if bold else Path(r"C:\Windows\Fonts\simsun.ttc"),
        Path(r"C:\Windows\Fonts\simsunb.ttf"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
    ]
    for choice in choices:
        if choice.exists():
            return ImageFont.truetype(str(choice), size=size)
    return ImageFont.load_default()


FONT_DIALOGUE = load_font(46, bold=True)
FONT_TITLE = load_font(66, bold=True)
FONT_SMALL = load_font(30)


def easing(value: float) -> float:
    value = min(max(value, 0.0), 1.0)
    return value * value * (3.0 - 2.0 * value)


def render_panel(panel: Image.Image, scene: int, progress: float) -> Image.Image:
    # Slow push-in with scene-specific pan directions.
    zoom = 1.035 + 0.055 * easing(progress)
    scaled_w = int(WIDTH * zoom)
    scaled_h = int(HEIGHT * zoom)
    resized = panel.resize((scaled_w, scaled_h), Image.Resampling.BICUBIC)
    max_x = scaled_w - WIDTH
    max_y = scaled_h - HEIGHT
    directions = [(-0.38, 0.10), (0.28, -0.12), (-0.22, 0.08), (0.0, -0.08)]
    dir_x, dir_y = directions[scene]
    center_x = max_x / 2 + dir_x * max_x * (progress - 0.5)
    center_y = max_y / 2 + dir_y * max_y * (progress - 0.5)
    left = int(min(max(center_x, 0), max_x))
    top = int(min(max(center_y, 0), max_y))
    frame = resized.crop((left, top, left + WIDTH, top + HEIGHT))
    frame = ImageEnhance.Color(frame).enhance(1.03)
    return frame


def text_for_time(t: float) -> tuple[str, str] | None:
    cues = [
        (0.35, 2.35, "title", "这一次，不算游戏。"),
        (2.55, 4.85, "dialogue", "优希也……今天先别说“爱你”。"),
        (5.10, 7.55, "dialogue", "我想说的，不是为了赢。"),
        (7.80, 9.95, "dialogue", "那我也不玩了。"),
        (10.20, 13.10, "dialogue", "我喜欢你。每一次都是真的。"),
        (13.35, 15.10, "dialogue", "……我也是。"),
        (15.35, 17.10, "dialogue", "我喜欢你。"),
        (17.35, 19.75, "final", "“爱你游戏”结束了。\n我们的故事，今天开始。"),
    ]
    for start, end, kind, text in cues:
        if start <= t <= end:
            return kind, text
    return None


def draw_centered_text(draw: ImageDraw.ImageDraw, xy_y: int, text: str,
                       font: ImageFont.FreeTypeFont, fill: tuple[int, int, int],
                       stroke: int = 3, spacing: int = 14) -> None:
    bbox = draw.multiline_textbbox((0, 0), text, font=font, spacing=spacing, align="center", stroke_width=stroke)
    text_width = bbox[2] - bbox[0]
    x = (WIDTH - text_width) // 2
    draw.multiline_text((x, xy_y), text, font=font, fill=fill, spacing=spacing,
                        align="center", stroke_width=stroke, stroke_fill=(28, 19, 25))


def add_overlay(frame: Image.Image, t: float) -> Image.Image:
    overlay = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    cue = text_for_time(t)
    if cue:
        kind, text = cue
        if kind == "title":
            draw_centered_text(draw, 108, text, FONT_TITLE, (255, 246, 238, 255), stroke=4)
            draw_centered_text(draw, 190, "原创同人短片", FONT_SMALL, (255, 224, 214, 238), stroke=2)
        elif kind == "final":
            draw.rounded_rectangle((350, 730, 1570, 965), radius=28, fill=(18, 9, 20, 125))
            draw_centered_text(draw, 765, text, FONT_TITLE, (255, 243, 237, 255), stroke=3, spacing=20)
        else:
            # Bottom gradient panel for legible dialogue while preserving the art.
            for row in range(260):
                alpha = int(168 * (row / 259) ** 1.8)
                draw.line((0, HEIGHT - 260 + row, WIDTH, HEIGHT - 260 + row), fill=(12, 7, 15, alpha))
            bbox = draw.textbbox((0, 0), text, font=FONT_DIALOGUE, stroke_width=3)
            x = (WIDTH - (bbox[2] - bbox[0])) // 2
            draw.text((x, 930), text, font=FONT_DIALOGUE, fill=(255, 249, 244, 255),
                      stroke_width=3, stroke_fill=(22, 13, 20))

    # Very subtle letterbox bars and a warm vignette.
    draw.rectangle((0, 0, WIDTH, 24), fill=(8, 4, 8, 215))
    draw.rectangle((0, HEIGHT - 24, WIDTH, HEIGHT), fill=(8, 4, 8, 215))
    composed = Image.alpha_composite(frame.convert("RGBA"), overlay).convert("RGB")
    return composed


def fade_from_black(frame: Image.Image, t: float) -> Image.Image:
    if t < 0.8:
        alpha = easing(t / 0.8)
        return Image.blend(Image.new("RGB", frame.size, (7, 4, 8)), frame, alpha)
    if t > DURATION - 0.9:
        alpha = easing((DURATION - t) / 0.9)
        return Image.blend(Image.new("RGB", frame.size, (7, 4, 8)), frame, alpha)
    return frame


def resolve_ffmpeg() -> Path:
    dependency_root = ROOT / "deps"
    sys.path.insert(0, str(dependency_root))
    import imageio_ffmpeg  # type: ignore
    return Path(imageio_ffmpeg.get_ffmpeg_exe())


def build_video(output: Path) -> None:
    if not STORYBOARD.exists():
        raise FileNotFoundError(f"Missing storyboard: {STORYBOARD}")
    make_music(MUSIC)
    panels = crop_panels(Image.open(STORYBOARD))
    ffmpeg = resolve_ffmpeg()
    command = [
        str(ffmpeg), "-y",
        "-f", "rawvideo", "-vcodec", "rawvideo", "-pix_fmt", "rgb24",
        "-s", f"{WIDTH}x{HEIGHT}", "-r", str(FPS), "-i", "-",
        "-i", str(MUSIC),
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
        "-shortest", "-movflags", "+faststart",
        "-metadata", "title=这一次，不算游戏。",
        "-metadata", "comment=Original fan-made short; original images, dialogue, and synthesized music.",
        str(output),
    ]
    process = subprocess.Popen(command, stdin=subprocess.PIPE)
    assert process.stdin is not None
    total_frames = int(DURATION * FPS)
    try:
        for frame_index in range(total_frames):
            t = frame_index / FPS
            scene = min(int(t / SCENE_LENGTH), 3)
            local = t - scene * SCENE_LENGTH
            progress = local / SCENE_LENGTH
            frame = render_panel(panels[scene], scene, progress)
            if scene < 3 and local > SCENE_LENGTH - FADE_LENGTH:
                blend = easing((local - (SCENE_LENGTH - FADE_LENGTH)) / FADE_LENGTH)
                next_frame = render_panel(panels[scene + 1], scene + 1, 0.0)
                frame = Image.blend(frame, next_frame, blend)
            frame = add_overlay(frame, t)
            frame = fade_from_black(frame, t)
            process.stdin.write(frame.tobytes())
    finally:
        process.stdin.close()
    return_code = process.wait()
    if return_code != 0:
        raise RuntimeError(f"ffmpeg exited with code {return_code}")


if __name__ == "__main__":
    output_path = versioned_output(desktop_folder(), "想要结束我爱你游戏_告白短片", ".mp4")
    build_video(output_path)
    print(output_path)
